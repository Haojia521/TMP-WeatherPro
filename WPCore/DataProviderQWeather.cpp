#include "DataProviderQWeather.h"
#include "utils.h"
#include "AppLocale.h"
#include "JsonValueHelper.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <functional>
#include <fstream>
#include <sstream>
#include <future>
#include <mutex>

#include <jwt-cpp/jwt.h>
#include <spdlog/spdlog.h>
#include <yyjson.h>

namespace
{
    constexpr std::chrono::seconds JWT_TIME_OFFSET{ 30 };
    constexpr std::chrono::seconds JWT_TIME_DURATION{ 900 };

    std::mutex mutex_gen_jwt;

    std::string generateJwt(const DataProviderQWeather::ConfigApp &cfg) {
        static std::chrono::system_clock::time_point timestamp;
        static std::string token_cache;
        static std::string prv_filepath_cache;

        std::scoped_lock lock{ mutex_gen_jwt };

        const auto now = std::chrono::system_clock::now();

        if (prv_filepath_cache == cfg.jwt_prv_key_file &&
            (now < timestamp + JWT_TIME_DURATION - JWT_TIME_OFFSET) && 
            !token_cache.empty()) {
            return token_cache;
        }

        prv_filepath_cache = cfg.jwt_prv_key_file;

        std::ifstream ifs(cfg.jwt_prv_key_file);

        std::string private_key;
        if (ifs.is_open()) {
            private_key.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

            ifs.close();
        } else {
            spdlog::error(tr::txt(tr::TID::ERR_JWT_CANNOT_OPEN_PRV_FILE));
            return {};
        }

        try {
            auto token = jwt::create()
                .set_algorithm("EdDSA")
                .set_key_id(cfg.credential_id)
                .set_issuer(cfg.developer_id)
                .set_subject(cfg.project_id)
                .set_issued_at(now - JWT_TIME_OFFSET)
                .set_expires_at(now + JWT_TIME_DURATION)
                .sign(jwt::algorithm::ed25519("", private_key));

            timestamp = now;
            token_cache = std::move(token);
        }
        catch (const std::exception &e) {
            spdlog::error(e.what());
            token_cache.clear();
        }

        return token_cache;
    }

    std::string formatErrorV2(yyjson_val *j_err)
    {
        auto status = yyjson_get_int(yyjson_obj_get(j_err, "status"));
        auto title = jvh::getString(j_err, "title");
        auto detail = jvh::getString(j_err, "detail");

        return std::format("[{}] {} ({})", status, title, detail);
    }

    std::string timestamp_string_yyyymmdd_hhmm(std::int64_t sec) {
        using namespace std::chrono;

        return std::format("{:%F %R}", sys_seconds{ seconds{sec} });
    }

    std::string parseValueUnit(yyjson_val *j_parent, const char *key, int p = 1) {
        auto *j_obj = yyjson_obj_get(j_parent, key);

        p = std::clamp(p, 0, 4);

        const auto fmt_str = std::format("{{:.{}f}}{{}}", p);
        const auto value = jvh::getNumber(j_obj, "value");
        const auto unit = jvh::getStringView(j_obj, "unit");
        return std::vformat(fmt_str, std::make_format_args(value, unit));
    }

    std::string formatWind(const std::string_view direction,
                           const std::string_view speed,
                           const std::string_view scale,
                           const bool use_scale) {
        auto wind_direction =
            std::vformat(tr::txt(tr::TID::FMT_TMP_WIND_DIRECTION),
                         std::make_format_args(direction));
        auto wind_strength = use_scale ?
            std::vformat(tr::txt(tr::TID::FMT_TMP_WIND_SCALE),
                         std::make_format_args(scale)) :
            std::string{ speed };
        return std::format("{} {}", wind_direction, wind_strength);
    }

    using ConfigApp = DataProviderQWeather::ConfigApp;
    using RealtimeWeather = DataProviderQWeather::RealtimeWeather;
    using ForecastedWeather = DataProviderQWeather::ForecastedWeather;
    using AirQualityIndex = DataProviderQWeather::AirQualityIndex;
    using RealtimeAirQuality = DataProviderQWeather::RealtimeAirQuality;
    using WeatherAlert = DataProviderQWeather::WeatherAlert;
    using RealtimeWeatherAlerts = DataProviderQWeather::RealtimeWeatherAlerts;

    bool queryFrame(const std::string &path, utils::HttpParams &params,
                    const ConfigApp &cfg,
                    const std::function<void(yyjson_val*)> &func)
    {
        bool succeed{ false };

        utils::HttpHeaders headers;

        if (cfg.enable_jwt) {
            auto jwt = generateJwt(cfg);
            if (jwt.empty()) {
                return false;
            }

            headers.data.emplace("Authorization", std::format("Bearer {}", jwt));
        } else {
            params.data.emplace("key", cfg.app_key);
        }

        params.data.emplace("lang", tr::txt(tr::TID::LC_QWEATHER));

        const auto host = std::format("https://{}", cfg.api_host);
        std::string content;

        auto status_code = utils::internetGetWithRetry(host, path, content, params, headers);

        if (!content.empty()) {
            SPDLOG_DEBUG(content);

            std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                yyjson_read(content.c_str(), content.size(), 0),
                [](yyjson_doc *p) { yyjson_doc_free(p); }
            );

            if (doc != nullptr) {
                auto *root = yyjson_doc_get_root(doc.get());

                if (status_code == 200) {
                    // compatible with error code v1
                    if (jvh::hasObject(root, "code") && jvh::getString(root, "code") != "200") {
                        spdlog::error("{}: {}", tr::txt(tr::TID::ERR_CODE), jvh::getString(root, "code"));
                    } else {
                        func(root);
                        succeed = true;
                    }
                } else {
                    // compatible with error code v2
                    if (jvh::hasObject(root, "error")) {
                        spdlog::error(formatErrorV2(yyjson_obj_get(root, "error")));
                    } else
                        spdlog::error("[{}] {}", status_code, tr::txt(tr::TID::ERR_UNKOWN));
                }
            } else
                spdlog::error(tr::txt(tr::TID::ERR_INVALID_JSON));
        } else
            spdlog::error(std::format("[{}] {}", status_code, tr::txt(tr::TID::ERR_INTERNET_EMPTY_RESPONSE)));

        return succeed;
    }

    RealtimeWeather queryRealtimeWeather(const Location &loc, const ConfigApp &cfg) {
        RealtimeWeather rt_weather{};

        if (loc.longitude.empty() || loc.latitude.empty()) {
            spdlog::error("{} (queryRealtimeWeather)", tr::txt(tr::TID::ERR_NO_LONG_LAT));
            return rt_weather;
        }

        const auto url_path = std::format("/weather/v1/current/{}/{}",
                                          loc.latitude, loc.longitude);

        auto parse_func = [&rt_weather](yyjson_val *j_root) {
            // temperature
            rt_weather.temp = parseValueUnit(j_root, "temperature");
            rt_weather.temp_feels_like = parseValueUnit(j_root, "feelsLike");
            
            // update time
            {
                auto now = std::chrono::floor<std::chrono::minutes>(
                    std::chrono::current_zone()->to_local(std::chrono::system_clock::now()));
                rt_weather.update_time = std::format("{:%H:%M}", now);
            }

            // weather condition
            auto *j_weather = yyjson_obj_get(j_root, "condition");
            rt_weather.weather_text = jvh::getStringView(j_weather, "text");
            rt_weather.weather_code = jvh::getStringView(j_weather, "code");

            // wind
            auto *j_wind = yyjson_obj_get(j_root, "wind");
            auto *j_wind_direction = yyjson_obj_get(j_wind, "direction");
            rt_weather.wind_direction = utils::get_direction_text(jvh::getStringView(j_wind_direction, "compass"));
            rt_weather.wind_speed = parseValueUnit(j_wind, "speed", 2);
            rt_weather.wind_scale = std::to_string(jvh::getInt(j_wind, "scale"));

            // humidity
            rt_weather.humidity = std::format("{:.0f}%", jvh::getReal(j_root, "humidity") * 100.0);

            // precipitation
            auto *j_precipitation = yyjson_obj_get(j_root, "precipitation");

            rt_weather.precipitation_type = jvh::getStringView(j_precipitation, "type");
            if (rt_weather.precipitation_type != "none") {
                rt_weather.precipitation_amount = parseValueUnit(j_precipitation, "amount", 2);
                rt_weather.precipitation_intensity = parseValueUnit(j_precipitation, "intensity", 2);
            }

            // uv index
            rt_weather.uv_index = std::to_string(jvh::getInt(j_root, "uvIndex"));
        };

        if (utils::HttpParams empty_params;
            !queryFrame(url_path, empty_params, cfg, parse_func)) {
            spdlog::error(tr::txt(tr::TID::ERR_QUERY_RTW_FAILED));
        }

        return rt_weather;
    }

    RealtimeAirQuality queryRealtimeAirQuality(const Location &loc, const ConfigApp &cfg) {
        RealtimeAirQuality rt_air{};

        if (loc.longitude.empty() || loc.latitude.empty()) {
            spdlog::error("{} (queryRealtimeAirQuality)", tr::txt(tr::TID::ERR_NO_LONG_LAT));
            return rt_air;
        }

        const std::string url_path = std::format("/airquality/v1/current/{}/{}", loc.latitude, loc.longitude);

        auto func = [&rt_air](yyjson_val *j_val) {
            {
                auto *j_arr_indexes = yyjson_obj_get(j_val, "indexes");

                size_t idx, max;
                yyjson_val *j_aqi_index;
                yyjson_arr_foreach(j_arr_indexes, idx, max, j_aqi_index) {
                    rt_air.indexes.push_back(
                        AirQualityIndex{
                            .code = jvh::getString(j_aqi_index, "code"),
                            .name = jvh::getString(j_aqi_index, "name"),
                            .aqi = jvh::getString(j_aqi_index, "aqiDisplay"),
                            .level = jvh::getString(j_aqi_index, "level"),
                            .category = jvh::getString(j_aqi_index, "category"),
                        });
                }
            }

            {
                auto *j_arr_pollutants = yyjson_obj_get(j_val, "pollutants");

                size_t idx, max;
                yyjson_val *j_pollutant;
                yyjson_arr_foreach(j_arr_pollutants, idx, max, j_pollutant) {
                    const auto code = jvh::getStringView(j_pollutant, "code");
                    if (code == "pm2p5") {
                        rt_air.pm2p5 = parseValueUnit(j_pollutant, "concentration", 2);
                    }
                    
                    if (code == "pm10") {
                        rt_air.pm10 = parseValueUnit(j_pollutant, "concentration", 2);
                    }
                }
            }
        };

        if (utils::HttpParams empty_params;
            !queryFrame(url_path, empty_params, cfg, func)) {
            spdlog::error(tr::txt(tr::TID::ERR_QUERY_RTAQ_FAILED));
        }

        return rt_air;
    }

    std::array<ForecastedWeather, 3> queryForcastedWeather3d(const Location &loc, const ConfigApp &cfg) {
        std::array<ForecastedWeather, 3> fc_weather_3d{};

        if (loc.longitude.empty() || loc.latitude.empty()) {
            spdlog::error("{} (queryForcastedWeather3d)", tr::txt(tr::TID::ERR_NO_LONG_LAT));
            return fc_weather_3d;
        }

        const auto url_path = std::format("/weather/v1/daily/{}/{}", loc.latitude, loc.longitude);

        auto parse_func = [&fc_weather_3d](yyjson_val *j_root) {
            auto parse_daily_data = [](yyjson_val *j_daily_data, ForecastedWeather &fc_weather) {
                // temperature
                fc_weather.temp_max = parseValueUnit(j_daily_data, "temperatureMax");
                fc_weather.temp_min = parseValueUnit(j_daily_data, "temperatureMin");

                // uv index
                fc_weather.uv_index = std::to_string(jvh::getInt(j_daily_data, "uvIndexMax"));

                auto parse_data_block = [](yyjson_val *j_block, ForecastedWeather::DataBlock &data_block) {
                    // temperature
                    data_block.temp_max = parseValueUnit(j_block, "temperatureMax");
                    data_block.temp_min = parseValueUnit(j_block, "temperatureMin");

                    // weather
                    auto *j_weather = yyjson_obj_get(j_block, "condition");
                    data_block.weather_text = jvh::getStringView(j_weather, "text");
                    data_block.weather_code = jvh::getStringView(j_weather, "code");

                    // humidity
                    data_block.humidity = std::format("{:.0f}%", jvh::getReal(j_block, "humidity") * 100.0);

                    // wind
                    auto *j_wind = yyjson_obj_get(j_block, "wind");
                    auto *j_wind_direction = yyjson_obj_get(j_wind, "direction");
                    data_block.wind_direction = utils::get_direction_text(jvh::getStringView(j_wind_direction, "compass"));
                    data_block.wind_speed = parseValueUnit(j_wind, "speed");
                    data_block.wind_scale = std::to_string(jvh::getInt(j_wind, "scale"));

                    // precipitation
                    auto *j_precipitation = yyjson_obj_get(j_block, "precipitation");
                    data_block.precipitation_type = jvh::getStringView(j_precipitation, "type");
                    if (data_block.precipitation_type != "none") {
                        data_block.precipitation_amount = parseValueUnit(j_precipitation, "amount", 2);
                        data_block.precipitation_prob = std::format("{:.0f}%", jvh::getReal(j_precipitation, "probability") * 100.0);
                    }
                };

                // day time
                auto *j_day = yyjson_obj_get(j_daily_data, "daytime");
                parse_data_block(j_day, fc_weather.day_time);

                // night time
                auto *j_night = yyjson_obj_get(j_daily_data, "nighttime");
                parse_data_block(j_night, fc_weather.night_time);
            };

            auto *j_arr_days = yyjson_obj_get(j_root, "days");
            const auto num_days = std::min(yyjson_arr_size(j_arr_days), fc_weather_3d.size());
            for (size_t i = 0; i < num_days; ++i) {
                parse_daily_data(yyjson_arr_get(j_arr_days, i), fc_weather_3d[i]);
            }
        };

        utils::HttpParams params;
        params.data.emplace("days", "3");

        if (!queryFrame(url_path, params, cfg, parse_func)) {
            spdlog::error(tr::txt(tr::TID::ERR_QUERY_FCW3D_FAILED));
        }

        return fc_weather_3d;
    }

    RealtimeWeatherAlerts queryRealtimeWeatherAlerts(const Location &loc, const ConfigApp &cfg) {
        RealtimeWeatherAlerts rt_alerts{};

        if (loc.longitude.empty() || loc.latitude.empty()) {
            spdlog::error("{} (queryRealtimeWeatherAlerts)", tr::txt(tr::TID::ERR_NO_LONG_LAT));
            return rt_alerts;
        }

        const std::string url_path = std::format("/weatheralert/v1/current/{}/{}", loc.latitude, loc.longitude);

        auto func = [&rt_alerts](yyjson_val *j_val) {
            auto *j_metadata = yyjson_obj_get(j_val, "metadata");
            if (yyjson_get_bool(yyjson_obj_get(j_metadata, "zeroResult"))) {
                return;
            }

            auto extractAlertInfo = [](yyjson_val *j_val_alt) {
                WeatherAlert wa;

                wa.sender_name = jvh::getString(j_val_alt, "senderName");
                wa.issued_time = timestamp_string_yyyymmdd_hhmm(
                    utils::parse_iso_datetime_to_local_seconds(
                        jvh::getString(j_val_alt, "issuedTime")
                    )
                );
                wa.severity = jvh::getString(j_val_alt, "severity");

                yyjson_val *j_color = yyjson_obj_get(j_val_alt, "color");
                wa.color_code = jvh::getString(j_color, "code");
                {
                    int r = yyjson_get_int(yyjson_obj_get(j_color, "red"));
                    int g = yyjson_get_int(yyjson_obj_get(j_color, "green"));
                    int b = yyjson_get_int(yyjson_obj_get(j_color, "blue"));
                    const double alpha = yyjson_get_num(yyjson_obj_get(j_color, "alpha"));
                    
                    int a = static_cast<int>(std::lerp(0.0, 255.0, std::clamp(alpha, 0.0, 1.0)));

                    wa.color = std::format("{:x}{:x}{:x}{:x}", r, g, b, a);
                }

                wa.expire_time = timestamp_string_yyyymmdd_hhmm(
                    utils::parse_iso_datetime_to_local_seconds(
                        jvh::getString(j_val_alt, "expireTime")
                    )
                );
                wa.headline = jvh::getString(j_val_alt, "headline");
                wa.description = jvh::getString(j_val_alt, "description");

                return wa;
            };

            {
                auto *j_arr_alerts = yyjson_obj_get(j_val, "alerts");
                size_t idx, max;
                yyjson_val *j_alert;
                yyjson_arr_foreach(j_arr_alerts, idx, max, j_alert) {
                    rt_alerts.alerts.push_back(extractAlertInfo(j_alert));
                }
            }

            {
                auto *j_arr_attributions = yyjson_obj_get(j_metadata, "attributions");
                size_t idx, max;
                yyjson_val *j_attrib;
                yyjson_arr_foreach(j_arr_attributions, idx, max, j_attrib) {
                    rt_alerts.attributions.emplace_back(yyjson_get_str(j_attrib));
                }
            }
        };

        if (utils::HttpParams empty_params;
            !queryFrame(url_path, empty_params, cfg, func)) {
            spdlog::error(tr::txt(tr::TID::ERR_QUERY_RTWA_FAILED));
        }

        return rt_alerts;
    }

    std::string formatAlerts(const DataProviderQWeather::RealtimeWeatherAlerts &rtw_alerts) {
        if (rtw_alerts.alerts.empty()) {
            return {};
        }
        
        std::ostringstream oss;
        for (const auto &alert : rtw_alerts.alerts) {
            oss << alert.headline << "\n";
            oss << "(" << alert.issued_time << " ~ " << alert.expire_time << ")" << "\n";
            oss << alert.description << "\n" << "\n";
        }

        for (const auto &attrib : rtw_alerts.attributions) {
            oss << attrib << "\n";
        }

        return oss.str();
    }

    bool queryLocations(const std::string &query, const DataProviderQWeather::ConfigApp &cfg, Locations &queried_locations) {
        constexpr std::string_view url_path{ "/geo/v2/city/lookup" };

        queried_locations.clear();

        utils::HttpParams url_params;
        url_params.data.emplace("location", query);

        auto func = [&queried_locations](yyjson_val *j_val) {
            auto *j_arr_locations = yyjson_obj_get(j_val, "location");

            auto extractLocationInfo = [](yyjson_val *j_val) {
                Location loc{
                    .id = jvh::getString(j_val, "id"),
                    .name = jvh::getString(j_val, "name"),
                    .administrative_ownership =
                        std::format("{}-{}", jvh::getString(j_val, "adm2"), jvh::getString(j_val, "adm1")),
                    .longitude = jvh::getString(j_val, "lon"),
                    .latitude = jvh::getString(j_val, "lat"),
                };

                std::string short_lon, short_lat;

                try {
                    short_lon = std::format("{:.2f}", std::stod(loc.longitude));
                    short_lat = std::format("{:.2f}", std::stod(loc.latitude));
                } catch (...) {
                    short_lon.clear();
                    short_lat.clear();
                }

                if (!short_lon.empty() && !short_lat.empty()) {
                    loc.longitude = short_lon;
                    loc.latitude = short_lat;
                }

                return loc;
            };

            size_t idx, max;
            yyjson_val *j_loc;
            yyjson_arr_foreach(j_arr_locations, idx, max, j_loc) {
                queried_locations.push_back(extractLocationInfo(j_loc));
            }
        };

        if (queryFrame(std::string{ url_path }, url_params, cfg, func)) {
            return true;
        }

        spdlog::error(tr::txt(tr::TID::ERR_QUERY_GEOCODING_FAILED));
        return false;
    }
}

DataProviderQWeather::WeatherDataBlock::WeatherDataBlock(const ConfigFormatting &cfg_fmt) : config(cfg_fmt) {}

std::string DataProviderQWeather::WeatherDataBlock::getWeatherSummary() const {
    const auto &cfg = config.get();
    std::ostringstream oss;

    // weather & temprature
    oss << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::WEATHER_TEXT) << " "
        << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::TEMPERATURE) << " "
        << "(" << realtime_weather.update_time << ")";

    // wind
    if (cfg.show_realtime_wind) {
        const auto wind = getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::WIND);
        oss << "\n" << std::vformat(tr::txt(tr::TID::FMT_TMP_WIND),
                                    std::make_format_args(wind));
    }

    // humidity
    if (cfg.show_realtime_humidity) {
        oss << std::format(" {}: {}", tr::txt(tr::TID::FMT_HUMIDITY),
                           getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::HUMIDITY));
    }

    // uv index
    if (cfg.show_realtime_uvi) {
        oss << std::format(" UVI: {}", getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::UV_INDEX));
    }

    // air quality
    oss << "\n"
        << std::format("{}: {}", tr::txt(tr::TID::FMT_AIR_QUALITY),
                       getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::AIR_QUALITY));
    if (cfg.show_realtime_pm2p5) {
        oss << " PM2.5: " << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::AIR_PM2P5);
    }
    if (cfg.show_realtime_pm10) {
        oss << " PM10: " << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::AIR_PM10);
    }

    // precipitation
    if (cfg.show_realtime_percipitation) {
        if (!realtime_weather.precipitation_intensity.empty()) {
            oss << "\n"
                << std::format("{}: {}",
                               tr::txt(tr::TID::FMT_PRECIPITATION_INTENSITY),
                               getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::PRECIPITATION));
        }

        if (!realtime_weather.precipitation_amount.empty()) {
            oss << "\n"
                << std::format("{}: {}",
                               tr::txt(tr::TID::FMT_PRECIPITATION_IN_1H),
                               realtime_weather.precipitation_amount);
        }
    }

    // weather alerts
    for (const auto &alert : weather_alerts.alerts) {
        oss << "\n" << std::format("[!] {} ({})", alert.headline, alert.issued_time);
    }

    // forecasted weather
    auto fcw_formatter = [&oss, &cfg, this](WeatherTimeSlot wt, std::string_view str_wt, const ForecastedWeather &fcw) {
        oss << "\n" << str_wt << ": ";
        
        if (cfg.show_forecasted_day_night_details) {
            oss << getWeatherItem(wt, WeatherItem::TEMPERATURE);
            if (cfg.show_forecasted_uv_index) {
                oss << " " << tr::txt(tr::TID::FMT_UVI) << ": " << getWeatherItem(wt, WeatherItem::UV_INDEX);
            }

            auto details_formatter = [&oss, &cfg](std::string_view str_time, const ForecastedWeather::DataBlock &data_block)
            {
                oss << "\n"
                    << std::format("[{}] {} {}~{}",
                                   str_time,
                                   data_block.weather_text, data_block.temp_min, data_block.temp_max);

                if (cfg.show_forecasted_wind) {
                    oss << " " << formatWind(data_block.wind_direction, data_block.wind_speed, data_block.wind_scale,
                                             cfg.show_forecasted_wind_scale);
                }

                if (cfg.show_forecasted_humidity) {
                    oss << std::format(" {}: {}", tr::txt(tr::TID::FMT_HUMIDITY), data_block.humidity);
                }

                if (cfg.show_forecasted_precipitation) {
                    if (data_block.precipitation_amount.empty()) {
                        oss << std::format(" {}: {}",
                                           tr::txt(tr::TID::FMT_PRECIPITATION),
                                           utils::get_precipitation_text(data_block.precipitation_type));
                    } else {
                        oss << std::format(" {}: {} {} {}",
                                           tr::txt(tr::TID::FMT_PRECIPITATION), 
                                           data_block.precipitation_prob,
                                           utils::get_precipitation_text(data_block.precipitation_type),
                                           data_block.precipitation_amount);
                    }
                }
            };

            // day
            details_formatter(tr::txt(tr::TID::FMT_DAY_TIME), fcw.day_time);
            // night
            details_formatter(tr::txt(tr::TID::FMT_NIGHT_TIME), fcw.night_time);
        } else {
            oss << getWeatherItem(wt, WeatherItem::WEATHER_TEXT)
                << " " << getWeatherItem(wt, WeatherItem::TEMPERATURE);
            if (cfg.show_forecasted_uv_index) {
                oss << " " << tr::txt(tr::TID::FMT_UVI) << ": " << getWeatherItem(wt, WeatherItem::UV_INDEX);
            }
        }
    };

    fcw_formatter(WeatherTimeSlot::TODAY, tr::txt(tr::TID::FMT_TODAY), fc_weather_3d[0]);
    fcw_formatter(WeatherTimeSlot::TOMMROW, tr::txt(tr::TID::FMT_TOMORROW), fc_weather_3d[1]);
    fcw_formatter(WeatherTimeSlot::DAY_AFTER_TOMMROW, tr::txt(tr::TID::FMT_DAT_AFTER_TOMORROW), fc_weather_3d[2]);

    oss << "\n";

    return oss.str();
}

std::string DataProviderQWeather::WeatherDataBlock::getWeatherItem(WeatherTimeSlot time_slot, WeatherItem item) const {
    const auto &cfg = config.get();

    if (time_slot == WeatherTimeSlot::REALTIME) {
        if (item == WeatherItem::TEMPERATURE) {
            if (cfg.show_realtime_temp_feels_like) {
                return realtime_weather.temp_feels_like;
            } else {
                return realtime_weather.temp;
            }
        } else if (item == WeatherItem::WEATHER_TEXT) {
            return realtime_weather.weather_text;
        } else if (item == WeatherItem::WEATHER_CODE) {
            return realtime_weather.weather_code;
        } else if (item == WeatherItem::HUMIDITY) {
            return realtime_weather.humidity;
        } else if (item == WeatherItem::WIND) {
            auto wind_direction =
                std::vformat(tr::txt(tr::TID::FMT_TMP_WIND_DIRECTION),
                             std::make_format_args(realtime_weather.wind_direction));
            auto wind_strength = cfg.show_realtime_wind_scale ?
                std::vformat(tr::txt(tr::TID::FMT_TMP_WIND_SCALE),
                             std::make_format_args(realtime_weather.wind_scale)) :
                realtime_weather.wind_speed;
            return std::format("{} {}", wind_direction, wind_strength);
        } else if (item == WeatherItem::UV_INDEX) {
            return realtime_weather.uv_index;
        } else if (item == WeatherItem::PRECIPITATION) {
            auto precipitation_text = utils::get_precipitation_text(realtime_weather.precipitation_type);
            if (!realtime_weather.precipitation_intensity.empty()) {
                return std::format("{} {}", precipitation_text, realtime_weather.precipitation_intensity);
            }
            return std::string{ precipitation_text };
        } else if (item == WeatherItem::AIR_QUALITY) {
            if (!realtime_air_quality.indexes.empty()) {
                const auto &aqi = realtime_air_quality.indexes.back();
                return std::format("{}({}: {})", aqi.category, aqi.name, aqi.aqi);
            }
        } else if (item == WeatherItem::AIR_PM2P5) {
            return realtime_air_quality.pm2p5;
        } else if (item == WeatherItem::AIR_PM10) {
            return realtime_air_quality.pm10;
        } else if (item == WeatherItem::ALERTS) {
            return formatAlerts(weather_alerts);
        }
    } else {
        auto getForecastedWeatherContent = [&cfg](const ForecastedWeather &fcw, WeatherItem item_fc) -> std::string {
            if (item_fc == WeatherItem::TEMPERATURE) {
                return std::format("{}~{}", fcw.temp_min, fcw.temp_max);
            } else if (item_fc == WeatherItem::WEATHER_TEXT) {
                return fcw.day_time.weather_text == fcw.night_time.weather_text ?
                    fcw.day_time.weather_text :
                    std::format("{}~{}", fcw.day_time.weather_text, fcw.night_time.weather_text);
            } else if (item_fc == WeatherItem::WEATHER_CODE) {
                return fcw.day_time.weather_code;
            } else if (item_fc == WeatherItem::HUMIDITY) {
                return fcw.day_time.humidity;
            } else if (item_fc == WeatherItem::WIND) {
                auto wind_direction =
                    std::vformat(tr::txt(tr::TID::FMT_TMP_WIND_DIRECTION),
                                 std::make_format_args(fcw.day_time.wind_direction));
                auto wind_strength = cfg.show_realtime_wind_scale ?
                    std::vformat(tr::txt(tr::TID::FMT_TMP_WIND_SCALE),
                                 std::make_format_args(fcw.day_time.wind_scale)) :
                    fcw.day_time.wind_speed;
                return std::format("{} {}", wind_direction, wind_strength);
            } else if (item_fc == WeatherItem::PRECIPITATION) {
                auto precipitation_text = utils::get_precipitation_text(fcw.day_time.precipitation_type);
                if (!fcw.day_time.precipitation_amount.empty()) {
                    return std::format("{} {} {}",
                                       fcw.day_time.precipitation_prob,
                                       precipitation_text,
                                       fcw.day_time.precipitation_amount);
                }
                return std::string{ precipitation_text };
            } else if (item_fc == WeatherItem::UV_INDEX) {
                return fcw.uv_index;
            } else {
                return {};
            }
        };

        if (time_slot == WeatherTimeSlot::TODAY) {
            return getForecastedWeatherContent(fc_weather_3d[0], item);
        } else if (time_slot == WeatherTimeSlot::TOMMROW) {
            return getForecastedWeatherContent(fc_weather_3d[1], item);
        } else if (time_slot == WeatherTimeSlot::DAY_AFTER_TOMMROW) {
            return getForecastedWeatherContent(fc_weather_3d[2], item);
        }
    }

    return {};
}

bool DataProviderQWeather::validateApiAuthentication() const
{
    if (config_app.api_host.empty()) {
        spdlog::error(tr::txt(tr::TID::ERR_AUTH_NO_API_HOST));
        return false;
    }

    if (config_app.enable_jwt) {
        if (config_app.developer_id.empty()) {
            spdlog::error(tr::txt(tr::TID::ERR_AUTH_JWT_NO_DEV_ID));
            return false;
        }

        if (config_app.project_id.empty()) {
            spdlog::error(tr::txt(tr::TID::ERR_AUTH_JWT_NO_PROJ_ID));
            return false;
        }

        if (config_app.credential_id.empty()) {
            spdlog::error(tr::txt(tr::TID::ERR_AUTH_JWT_NO_CRED_ID));
            return false;
        }
    } else {
        if (config_app.app_key.empty()) {
            spdlog::error(tr::txt(tr::TID::ERR_AUTH_NO_APP_KEY));
            return false;
        }
    }

    return true;
}

WeatherDataCPtr DataProviderQWeather::getWeatherData(const Location &loc) const {
    spdlog::info("qweather api: fetch weather data");

    if (!validateApiAuthentication()) {
        return nullptr;
    }

    if (loc.longitude.empty() || loc.latitude.empty()) {
        spdlog::error(tr::txt(tr::TID::ERR_NO_LONG_LAT));
        return nullptr;
    }

    auto fut_rt_weather = std::async(std::launch::async, [this, &loc] {
        return queryRealtimeWeather(loc, config_app);
    });

    auto fut_rt_air = std::async(std::launch::async, [this, &loc] {
        return queryRealtimeAirQuality(loc, config_app);
    });

    auto fut_fc_weather_3d = std::async(std::launch::async, [this, &loc] {
        return queryForcastedWeather3d(loc, config_app);
    });

    auto fut_alerts = std::async(std::launch::async, [this, &loc] {
        return queryRealtimeWeatherAlerts(loc, config_app);
    });

    const auto data_block = std::make_shared<WeatherDataBlock>(config_fmt);
    data_block->realtime_weather = fut_rt_weather.get();
    data_block->realtime_air_quality = fut_rt_air.get();
    data_block->fc_weather_3d = fut_fc_weather_3d.get();
    data_block->weather_alerts = fut_alerts.get();

    return data_block;
}

bool DataProviderQWeather::geocodingDirect(const std::string &query, Locations &queried_locations) const {
    spdlog::info("qweather api: query locations by direct geocoding");

    if (!validateApiAuthentication()) {
        return false;
    }

    return queryLocations(query, config_app, queried_locations);
}

bool DataProviderQWeather::geocodingReverse(const std::string &latitude, const std::string &longitude,
                                            Locations &queried_locations) const {
    spdlog::info("qweather api: query locations by reverse geocoding");

    if (!validateApiAuthentication()) {
        return false;
    }

    const auto query = std::format("{},{}", longitude, latitude);
    if (queryLocations(query, config_app, queried_locations)) {
        // Replace the returned coordinates (usually only one for lat/lng lookup) with the function's 
        // input values to accurately record the specified location
        if (!queried_locations.empty()) {
            queried_locations[0].longitude = longitude;
            queried_locations[0].latitude = latitude;
        }
        return true;
    }

    return false;
}
