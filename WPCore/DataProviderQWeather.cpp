#include "DataProviderQWeather.h"
#include "Logger.h"
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
            Logger::instance().error(tr::txt(tr::TID::ERR_JWT_CANNOT_OPEN_PRV_FILE));
            return {};
        }

        try {
            auto token = jwt::create()
                .set_issued_at(now - JWT_TIME_OFFSET)
                .set_expires_at(now + JWT_TIME_DURATION)
                .set_subject(cfg.project_id)
                .set_header_claim("kid", jwt::claim(cfg.credential_id))
                .sign(jwt::algorithm::ed25519("", private_key));

            timestamp = now;
            token_cache = token;
        }
        catch (const std::exception &e) {
            Logger::instance().error(e.what());

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

    std::string timestamp_string_hhmm(std::int64_t sec) {
        using namespace std::chrono;

        const auto tp = sys_seconds{ seconds{sec} };
        const auto day_time = tp - floor<days>(tp);
        return std::format("{:%R}", hh_mm_ss{ day_time });
    }

    std::string timestamp_string_yyyymmdd_hhmm(std::int64_t sec) {
        using namespace std::chrono;

        return std::format("{:%F %R}", sys_seconds{ seconds{sec} });
    }

    bool queryFrame(const std::string &path, const utils::HttpParams &params,
                    const DataProviderQWeather::ConfigApp &cfg,
                    const std::function<void(yyjson_val*)> &func)
    {
        bool succeed{ false };

        utils::HttpParams http_params;
        http_params.data.insert(params.data.begin(), params.data.end());

        utils::HttpHeaders http_headers;

        if (cfg.enable_jwt) {
            auto jwt = generateJwt(cfg);
            if (jwt.empty()) {
                return false;
            }

            http_headers.data.emplace("Authorization", std::format("Bearer {}", jwt));
        } else {
            http_params.data.emplace("key", cfg.app_key);
        }

        //auto lang = std::string{ tr::txt(tr::TID::LC_QWEATHER) };
        http_params.data.emplace("lang", tr::txt(tr::TID::LC_QWEATHER));

        const auto host = std::format("https://{}", cfg.api_host);
        std::string content;

        auto status_code = utils::internetGetWithRetry(host, path, content, http_params, http_headers);

        if (!content.empty()) {
            std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                yyjson_read(content.c_str(), content.size(), 0),
                [](yyjson_doc *p) { yyjson_doc_free(p); }
            );

            if (doc != nullptr) {
                auto *root = yyjson_doc_get_root(doc.get());

                if (status_code == 200) {
                    // compatible with error code v1
                    if (jvh::hasObject(root, "code") && jvh::getString(root, "code") != "200") {
                        Logger::instance().error(
                            std::format("{}: {}", tr::txt(tr::TID::ERR_CODE), jvh::getString(root, "code")));
                    } else {
                        func(root);
                        succeed = true;
                    }
                } else {
                    // compatible with error code v2
                    if (jvh::hasObject(root, "error")) {
                        Logger::instance().error(formatErrorV2(yyjson_obj_get(root, "error")));
                    } else
                        Logger::instance().error(
                            std::format("[{}] {}", status_code, tr::txt(tr::TID::ERR_UNKOWN)));
                }
            } else
                Logger::instance().error(tr::txt(tr::TID::ERR_INVALID_JSON));
        } else
            Logger::instance().error(std::format("[{}] {}", status_code, tr::txt(tr::TID::ERR_INTERNET_EMPTY_RESPONSE)));

        return succeed;
    }

    DataProviderQWeather::RealtimeWeather queryRealtimeWeather(const Location &loc, const DataProviderQWeather::ConfigApp &cfg) {
        constexpr std::string_view url_path{ "/v7/weather/now" };

        DataProviderQWeather::RealtimeWeather rt_weather{};

        std::string query;
        if (!loc.latitude.empty() && !loc.longitude.empty()) {
            query = std::format("{},{}", loc.longitude, loc.latitude);
        } else {
            query = loc.id;
        }

        utils::HttpParams url_params;
        url_params.data.emplace("location", query);

        auto func = [&rt_weather](yyjson_val *j_val) {
            auto *now_obj = yyjson_obj_get(j_val, "now");

            rt_weather.temp = jvh::getString(now_obj, "temp");
            rt_weather.temp_feels_like = jvh::getString(now_obj, "feelsLike");
            rt_weather.update_time = timestamp_string_hhmm(
                utils::parse_iso_datetime_to_local_seconds(
                    jvh::getString(now_obj, "obsTime")
                )
            );
            rt_weather.weather_text = jvh::getString(now_obj, "text");
            rt_weather.weather_code = jvh::getString(now_obj, "icon");
            rt_weather.wind_direction = jvh::getString(now_obj, "windDir");
            rt_weather.wind_scale = jvh::getString(now_obj, "windScale");
            rt_weather.wind_speed = jvh::getString(now_obj, "windSpeed");
            rt_weather.humidity = jvh::getString(now_obj, "humidity");
        };

        if (!queryFrame(std::string{ url_path }, url_params, cfg, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_RTW_FAILED));
        }

        return rt_weather;
    }

    DataProviderQWeather::RealtimeAirQuality queryRealtimeAirQuality(const Location &loc, const DataProviderQWeather::ConfigApp &cfg) {
        DataProviderQWeather::RealtimeAirQuality rt_air{};

        if (loc.longitude.empty() || loc.latitude.empty()) {
            Logger::instance().error(std::format("{} (queryRealtimeAirQuality)", tr::txt(tr::TID::ERR_NO_LONG_LAT)));
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
                        DataProviderQWeather::AirQualityIndex{
                            .code = jvh::getString(j_aqi_index, "code"),
                            .name = jvh::getString(j_aqi_index, "name"),
                            .aqi = jvh::getString(j_aqi_index, "aqiDisplay"),
                            .level = jvh::getString(j_aqi_index, "level"),
                            .category = jvh::getString(j_aqi_index, "category"),
                        });
                }
            }

            auto extractPollutantConcentration = [](yyjson_val *j_val_pc, std::string &str_concentration) {
                auto *j_concentration = yyjson_obj_get(j_val_pc, "concentration");
                str_concentration = std::format("{:.2f}{}", 
                                                jvh::getNumber(j_concentration, "value"),
                                                jvh::getString(j_concentration, "unit"));
            };

            {
                auto *j_arr_pollutants = yyjson_obj_get(j_val, "pollutants");

                size_t idx, max;
                yyjson_val *j_pollutant;
                yyjson_arr_foreach(j_arr_pollutants, idx, max, j_pollutant) {
                    if (auto code = jvh::getString(j_pollutant, "code");
                        code == "pm2p5") {
                        extractPollutantConcentration(j_pollutant, rt_air.pm2p5);
                        } else if (code == "pm10") {
                            extractPollutantConcentration(j_pollutant, rt_air.pm10);
                        }
                }
            }
        };

        if (!queryFrame(url_path, utils::HttpParams{}, cfg, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_RTAQ_FAILED));
        }

        return rt_air;
    }

    std::array<DataProviderQWeather::ForecastedWeather, 3> queryForcastedWeather3d(const Location &loc, const DataProviderQWeather::ConfigApp &cfg) {
        constexpr std::string_view url_path{ "/v7/weather/3d" };

        std::array<DataProviderQWeather::ForecastedWeather, 3> fc_weather_3d{};

        std::string query;
        if (!loc.latitude.empty() && !loc.longitude.empty()) {
            query = std::format("{},{}", loc.longitude, loc.latitude);
        } else {
            query = loc.id;
        }

        utils::HttpParams url_params;
        url_params.data.emplace("location", query);

        auto func = [&fc_weather_3d](yyjson_val *j_val) {
            auto *daily_arr = yyjson_obj_get(j_val, "daily");

            auto getDailyInfo = [](yyjson_val *j_val_day, DataProviderQWeather::ForecastedWeather &w) {
                w.temp_max = jvh::getString(j_val_day, "tempMax");
                w.temp_min = jvh::getString(j_val_day, "tempMin");
                w.weather_day = jvh::getString(j_val_day, "textDay");
                w.weather_night = jvh::getString(j_val_day, "textNight");
                w.code_day = jvh::getString(j_val_day, "iconDay");
                w.code_night = jvh::getString(j_val_day, "iconNight");
                w.uv_index = jvh::getString(j_val_day, "uvIndex");
                w.humidity = jvh::getString(j_val_day, "humidity");
            };

            getDailyInfo(yyjson_arr_get(daily_arr, 0), fc_weather_3d[0]);
            getDailyInfo(yyjson_arr_get(daily_arr, 1), fc_weather_3d[1]);
            getDailyInfo(yyjson_arr_get(daily_arr, 2), fc_weather_3d[2]);
        };

        if (!queryFrame(std::string{ url_path }, url_params, cfg, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_FCW3D_FAILED));
        }

        return fc_weather_3d;
    }

    DataProviderQWeather::RealtimeWeatherAlerts queryRealtimeWeatherAlerts(const Location &loc, const DataProviderQWeather::ConfigApp &cfg) {
        DataProviderQWeather::RealtimeWeatherAlerts rt_alerts{};

        if (loc.longitude.empty() || loc.latitude.empty()) {
            Logger::instance().error(std::format("{} (queryRealtimeWeatherAlerts)", tr::txt(tr::TID::ERR_NO_LONG_LAT)));
            return rt_alerts;
        }

        const std::string url_path = std::format("/weatheralert/v1/current/{}/{}", loc.latitude, loc.longitude);

        auto func = [&rt_alerts](yyjson_val *j_val) {
            auto *j_metadata = yyjson_obj_get(j_val, "metadata");
            if (yyjson_get_bool(yyjson_obj_get(j_metadata, "zeroResult"))) {
                return;
            }

            auto extractAlertInfo = [](yyjson_val *j_val_alt) {
                DataProviderQWeather::WeatherAlert wa;

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

        if (!queryFrame(url_path, utils::HttpParams{}, cfg, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_RTWA_FAILED));
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
                return Location{
                    .id = jvh::getString(j_val, "id"),
                    .name = jvh::getString(j_val, "name"),
                    .administrative_ownership =
                        std::format("{}-{}", jvh::getString(j_val, "adm2"), jvh::getString(j_val, "adm1")),
                    .longitude = jvh::getString(j_val, "lon"),
                    .latitude = jvh::getString(j_val, "lat"),
                };
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

        Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_GEOCODING_FAILED));
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
        oss << "\n" << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::WIND);
    }

    // humidity
    if (cfg.show_realtime_humidity) {
        oss << std::format(" {}: {}", tr::txt(tr::TID::FMT_HUMIDITY),
                           getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::HUMIDITY));
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

    // weather alerts
    for (const auto &alert : weather_alerts.alerts) {
        oss << "\n" << std::format("[!] {} ({})", alert.headline, alert.issued_time);
    }

    // forecasted weather
    auto fcw_formatter = [&oss, &cfg, this](WeatherTimeSlot wt, std::string_view str_wt) {
        oss << "\n" << str_wt << ": " << getWeatherItem(wt, WeatherItem::WEATHER_TEXT)
            << " " << getWeatherItem(wt, WeatherItem::TEMPERATURE);
        if (cfg.show_forecasted_humidity) {
            oss << " " << tr::txt(tr::TID::FMT_HUMIDITY) << ": " << getWeatherItem(wt, WeatherItem::HUMIDITY);
        }
        if (cfg.show_forecasted_uv_index) {
            oss << " " << tr::txt(tr::TID::FMT_UVI) << ": " << getWeatherItem(wt, WeatherItem::UV_INDEX);
        }
    };

    fcw_formatter(WeatherTimeSlot::TODAY, tr::txt(tr::TID::FMT_TODAY));
    fcw_formatter(WeatherTimeSlot::TOMMROW, tr::txt(tr::TID::FMT_TOMORROW));
    fcw_formatter(WeatherTimeSlot::DAY_AFTER_TOMMROW, tr::txt(tr::TID::FMT_DAT_AFTER_TOMORROW));

    oss << "\n";

    return oss.str();
}

std::string DataProviderQWeather::WeatherDataBlock::getWeatherItem(WeatherTimeSlot time_slot, WeatherItem item) const {
    const auto &cfg = config.get();

    if (time_slot == WeatherTimeSlot::REALTIME) {
        if (item == WeatherItem::TEMPERATURE) {
            if (cfg.show_realtime_temp_feels_like) {
                return std::format("{}℃", realtime_weather.temp_feels_like);
            } else {
                return std::format("{}℃", realtime_weather.temp);
            }
        } else if (item == WeatherItem::WEATHER_TEXT) {
            return realtime_weather.weather_text;
        } else if (item == WeatherItem::WEATHER_CODE) {
            return realtime_weather.weather_code;
        } else if (item == WeatherItem::HUMIDITY) {
            return std::format("{} %", realtime_weather.humidity);
        } else if (item == WeatherItem::WIND) {
            auto wind_direction =
                std::vformat(tr::txt(tr::TID::FMT_TMP_WIND_DIRECTION),
                             std::make_format_args(realtime_weather.wind_direction));
            auto wind_strength = cfg.show_realtime_wind_scale ?
                std::vformat(tr::txt(tr::TID::FMT_TMP_WIND_SCALE),
                             std::make_format_args(realtime_weather.wind_scale)) :
                std::format("{}km/h", realtime_weather.wind_speed);
            return std::format("{} {}", realtime_weather.wind_direction, wind_strength);
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
        auto getForecastedWeatherContent = [](const ForecastedWeather &fcw, WeatherItem item_fc) -> std::string {
            if (item_fc == WeatherItem::TEMPERATURE) {
                return std::format("{}~{}℃", fcw.temp_min, fcw.temp_max);
            } else if (item_fc == WeatherItem::WEATHER_TEXT) {
                return fcw.weather_day == fcw.weather_night ?
                    fcw.weather_day :
                    std::format("{}~{}", fcw.weather_day, fcw.weather_night);
            } else if (item_fc == WeatherItem::WEATHER_CODE) {
                return fcw.code_day;
            } else if (item_fc == WeatherItem::HUMIDITY) {
                return std::format("{} %", fcw.humidity);
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
        Logger::instance().error(tr::txt(tr::TID::ERR_AUTH_NO_API_HOST));
        return false;
    }

    if (config_app.enable_jwt) {
        if (config_app.project_id.empty()) {
            Logger::instance().error(tr::txt(tr::TID::ERR_AUTH_JWT_NO_PROJ_ID));
            return false;
        }

        if (config_app.credential_id.empty()) {
            Logger::instance().error(tr::txt(tr::TID::ERR_AUTH_JWT_NO_CRED_ID));
            return false;
        }
    } else {
        if (config_app.app_key.empty()) {
            Logger::instance().error(tr::txt(tr::TID::ERR_AUTH_NO_APP_KEY));
            return false;
        }
    }

    return true;
}

WeatherDataCPtr DataProviderQWeather::getWeatherData(const Location &loc) const {
    if (!validateApiAuthentication()) {
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
    if (!validateApiAuthentication()) {
        return false;
    }

    return queryLocations(query, config_app, queried_locations);
}

bool DataProviderQWeather::geocodingReverse(const std::string &latitude, const std::string &longitude,
                                            Locations &queried_locations) const {
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
