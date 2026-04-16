#include "DataProviderQWeather.h"
#include "Logger.h"
#include "utils.h"
#include "AppLocale.h"

#include <array>
#include <algorithm>
#include <functional>
#include <fstream>
#include <sstream>

#include <jwt-cpp/jwt.h>
#include <yyjson.h>

namespace qw
{
    static constexpr std::chrono::seconds JWTTIMEOFFSET{ 30 };
    static constexpr std::chrono::seconds JWTTIMEDURATION{ 900 };

    static std::string generateJwt(const DataProviderQWeather::Config &cfg) {
        static std::chrono::system_clock::time_point timestamp;
        static std::string token_cache;

        auto now = std::chrono::system_clock::now();

        if ((now < timestamp + JWTTIMEDURATION - JWTTIMEOFFSET) && !token_cache.empty()) {
            return token_cache;
        }

        std::ifstream ifs(cfg.jwt_prv_key_file);

        std::string private_key;
        if (ifs.is_open()) {
            private_key.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

            ifs.close();
        } else {
            //Logger::instance().error(L"GenJWT: cannot open private key file");
            Logger::instance().error(tr::txt(tr::TextID::ERR_JWT_CANNOT_OPEN_PRV_FILE));
            return "";
        }

        try {
            auto token = jwt::create()
                .set_issued_at(now - JWTTIMEOFFSET)
                .set_expires_at(now + JWTTIMEDURATION)
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

    static std::string jsonGetStrValue(yyjson_val *j_val, const char *key) {
        auto *obj = yyjson_obj_get(j_val, key);
        if (obj == nullptr) return "";
        else return yyjson_get_str(obj);
    }

    static double jsonGetRealValue(yyjson_val *j_val, const char *key) {
        auto *obj = yyjson_obj_get(j_val, key);
        if (obj == nullptr) return 0.0;
        else return yyjson_get_real(obj);
    }

    static bool jsonHasObject(yyjson_val *j_val, const char *key) {
        return yyjson_obj_get(j_val, key) != nullptr;
    }

    static std::string formatErrorV2(yyjson_val *j_err)
    {
        auto status = yyjson_get_int(yyjson_obj_get(j_err, "status"));
        auto title = jsonGetStrValue(j_err, "title");
        auto detail = jsonGetStrValue(j_err, "detail");

        return std::format("[{}] {} ({})", status, title, detail);
    }

    static void addParameterToUrl(std::string &url, const std::string &param_key, const std::string &param_value) {
        if (param_key.empty()) {
            return;
        }
        
        if (url.find('?') == std::string::npos) {
            url += '?';
        }

        url += std::format("&{}={}", param_key, param_value);
    }

    static bool queryFrame(const std::string &host, const std::string &path, const DataProviderQWeather::Config &cfg,
                           std::function<void(yyjson_val*)> func)
    {
        bool succeed{ false };

        std::string url_path = path;

        std::unordered_map<std::string, std::string> http_headers;
        if (cfg.enable_jwt) {
            auto jwt = generateJwt(cfg);
            if (jwt.empty()) {
                return false;
            }

            http_headers.insert(std::make_pair("Authorization", std::format("Bearer {}", jwt)));
        } else {
            addParameterToUrl(url_path, "key", cfg.app_key);
        }

        std::string lang = tr::txt(tr::TextID::LC_QWEATHER);
        addParameterToUrl(url_path, "lang", lang);

        std::string content;
        auto status_code = utils::internetGet(host, url_path, content, http_headers);

        if (!content.empty()) {
            std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                yyjson_read(content.c_str(), content.size(), 0),
                [](yyjson_doc *p) { yyjson_doc_free(p); }
            );

            if (doc != nullptr) {
                auto *root = yyjson_doc_get_root(doc.get());

                if (status_code == 200) {
                    // compatible with error code v1
                    if (jsonHasObject(root, "code") && jsonGetStrValue(root, "code") != "200") {
                        Logger::instance().error(
                            std::format("{}: {}", tr::txt(tr::TextID::ERR_CODE), jsonGetStrValue(root, "code")));
                    } else {
                        func(root);
                        succeed = true;
                    }
                } else {
                    // compatible with error code v2
                    if (jsonHasObject(root, "error")) {
                        Logger::instance().error(formatErrorV2(yyjson_obj_get(root, "error")));
                    } else
                        //Logger::instance().error(std::format(L"[{}] unknown error", status_code));
                        Logger::instance().error(
                            std::format("[{}] {}", status_code, tr::txt(tr::TextID::ERR_UNKOWN)));
                }
            } else
                //Logger::instance().error(L"Invalid json content.");
                Logger::instance().error(tr::txt(tr::TextID::ERR_INVALID_JSON));
        } else
            //Logger::instance().error(std::format(L"[{}] {}", status_code, L"Internet error."));
            Logger::instance().error(std::format("[{}] {}", status_code, tr::txt(tr::TextID::ERR_INTERNET_EMPTY_RESPONSE)));

        return succeed;
    }

    static bool queryRealtimeWeather(const Location &loc, const DataProviderQWeather::Config &cfg,
                                     DataProviderQWeather::RealtimeWeather &rt_weather) {
        rt_weather = DataProviderQWeather::RealtimeWeather{};

        std::string query;
        if (!loc.latitude.empty() && !loc.longitude.empty()) {
            query = std::format("{},{}", loc.longitude, loc.latitude);
        } else {
            query = loc.id;
        }

        std::string url_host = std::format("https://{}", cfg.api_host);
        std::string url_path = std::format("/v7/weather/now?location={}", query);

        auto func = [&rt_weather](yyjson_val *j_val) {
            auto *now_obj = yyjson_obj_get(j_val, "now");

            rt_weather.temp = jsonGetStrValue(now_obj, "temp");
            rt_weather.temp_feels_like = jsonGetStrValue(now_obj, "feelsLike");
            rt_weather.update_time = jsonGetStrValue(now_obj, "obsTime").substr(11, 5);
            rt_weather.weather_text = jsonGetStrValue(now_obj, "text");
            rt_weather.weather_code = jsonGetStrValue(now_obj, "icon");
            rt_weather.wind_direction = jsonGetStrValue(now_obj, "windDir");
            rt_weather.wind_scale = jsonGetStrValue(now_obj, "windScale");
            rt_weather.wind_speed = jsonGetStrValue(now_obj, "windSpeed");
            rt_weather.humidity = jsonGetStrValue(now_obj, "humidity");
        };

        if (queryFrame(url_host, url_path, cfg, func)) {
            return true;
        } else {
            //Logger::instance().error(L"QueryRealtimeWeather failed");
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_RTW_FAILED));
            return false;
        }
    }

    static bool queryRealtimeAirQuality(const Location &loc, const DataProviderQWeather::Config &cfg,
                                        DataProviderQWeather::RealtimeAirQuality &rt_air) {
        rt_air = DataProviderQWeather::RealtimeAirQuality{};

        if (loc.longitude.empty() || loc.latitude.empty()) {
            //Logger::instance().error(L"queryRealtimeAirQuality: no longitude or latitude");
            Logger::instance().error(std::format("{} (queryRealtimeAirQuality)",
                                                 tr::txt(tr::TextID::ERR_NO_LONG_LAT)));
            return false;
        }

        std::string url_host = std::format("https://{}", cfg.api_host);
        std::string url_path = std::format("/airquality/v1/current/{}/{}", loc.latitude, loc.longitude);

        auto func = [&rt_air](yyjson_val *j_val) {
            auto *j_arr_indexes = yyjson_obj_get(j_val, "indexes");

            size_t idx{ 0 }, max{ 0 };
            yyjson_val *j_aqi_index{ nullptr };
            yyjson_arr_foreach(j_arr_indexes, idx, max, j_aqi_index) {
                rt_air.indexes.push_back(
                    DataProviderQWeather::AirQualityIndex{
                        .code = jsonGetStrValue(j_aqi_index, "code"),
                        .name = jsonGetStrValue(j_aqi_index, "name"),
                        .aqi = jsonGetStrValue(j_aqi_index, "aqiDisplay"),
                        .level = jsonGetStrValue(j_aqi_index, "level"),
                        .category = jsonGetStrValue(j_aqi_index, "category"),
                    });
            }

            auto *j_arr_pollutants = yyjson_obj_get(j_val, "pollutants");

            auto extractPollutantConcentration = [](yyjson_val *j_val, std::string &str_concentration) {
                auto *j_concentration = yyjson_obj_get(j_val, "concentration");
                str_concentration = std::format("{:.2f}{}", 
                                                jsonGetRealValue(j_concentration, "value"),
                                                jsonGetStrValue(j_concentration, "unit"));
            };

            idx = max = 0;
            yyjson_val *j_pollutant{ nullptr };
            yyjson_arr_foreach(j_arr_pollutants, idx, max, j_pollutant) {
                auto code = jsonGetStrValue(j_pollutant, "code");

                if (code == "pm2p5") {
                    extractPollutantConcentration(j_pollutant, rt_air.pm2p5);
                } else if (code == "pm10") {
                    extractPollutantConcentration(j_pollutant, rt_air.pm10);
                }
            }
        };

        if (queryFrame(url_host, url_path, cfg, func)) {
            return true;
        } else {
            //Logger::instance().error(L"QueryRealtimeWeather failed");
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_RTAQ_FAILED));
            return false;
        }
    }

    static bool queryForcastedWeather3d(const Location &loc, const DataProviderQWeather::Config &cfg,
                                        std::array<DataProviderQWeather::ForecastedWeather, 3> &fc_weather_3d) {
        fc_weather_3d = std::array<DataProviderQWeather::ForecastedWeather, 3>{};

        std::string query;
        if (!loc.latitude.empty() && !loc.longitude.empty()) {
            query = std::format("{},{}", loc.longitude, loc.latitude);
        } else {
            query = loc.id;
        }

        std::string url_host = std::format("https://{}", cfg.api_host);
        std::string url_path = std::format("/v7/weather/3d?location={}", query);

        auto func = [&fc_weather_3d](yyjson_val *j_val) {
            auto *daily_arr = yyjson_obj_get(j_val, "daily");

            auto getDailyInfo = [](yyjson_val *j_val, DataProviderQWeather::ForecastedWeather &w) {
                w.temp_max = jsonGetStrValue(j_val, "tempMax");
                w.temp_min = jsonGetStrValue(j_val, "tempMin");
                w.weather_day = jsonGetStrValue(j_val, "textDay");
                w.weather_night = jsonGetStrValue(j_val, "textNight");
                w.code_day = jsonGetStrValue(j_val, "iconDay");
                w.code_night = jsonGetStrValue(j_val, "iconNight");
                w.uv_index = jsonGetStrValue(j_val, "uvIndex");
                w.humidity = jsonGetStrValue(j_val, "humidity");
            };

            getDailyInfo(yyjson_arr_get(daily_arr, 0), fc_weather_3d[0]);
            getDailyInfo(yyjson_arr_get(daily_arr, 1), fc_weather_3d[1]);
            getDailyInfo(yyjson_arr_get(daily_arr, 2), fc_weather_3d[2]);
        };

        if (queryFrame(url_host, url_path, cfg, func)) {
            return true;
        } else {
            //errors.push_back(L"QueryForecastWeather failed");
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_FCW3D_FAILED));
            return false;
        }
    }

    static bool queryRealtimeWeatherAlerts(const Location &loc, const DataProviderQWeather::Config &cfg,
                                           DataProviderQWeather::RealtimeWeatherAlerts &rt_alerts) {
        rt_alerts = DataProviderQWeather::RealtimeWeatherAlerts{};

        if (loc.longitude.empty() || loc.latitude.empty()) {
            //Logger::instance().error(L"queryRealtimeWeatherAlerts: no longitude or latitude");
            Logger::instance().error(std::format("{} (queryRealtimeWeatherAlerts)",
                                                 tr::txt(tr::TextID::ERR_NO_LONG_LAT)));
            return false;
        }

        std::string url_host = std::format("https://{}", cfg.api_host);
        std::string url_path = std::format("/weatheralert/v1/current/{}/{}", loc.latitude, loc.longitude);

        auto func = [&rt_alerts](yyjson_val *j_val) {
            auto *j_metadata = yyjson_obj_get(j_val, "metadata");
            if (yyjson_get_bool(yyjson_obj_get(j_metadata, "zeroResult"))) {
                return;
            }

            auto extractAlertInfo = [](yyjson_val *j_val) {
                DataProviderQWeather::WeatherAlert wa;

                wa.sender_name = jsonGetStrValue(j_val, "senderName");
                wa.issued_time = jsonGetStrValue(j_val, "issuedTime").substr(0, 16).replace(10, 1, " ");
                wa.severity = jsonGetStrValue(j_val, "severity");

                yyjson_val *j_color = yyjson_obj_get(j_val, "color");
                wa.color_code = jsonGetStrValue(j_color, "code");
                {
                    int r = yyjson_get_int(yyjson_obj_get(j_color, "red"));
                    int g = yyjson_get_int(yyjson_obj_get(j_color, "green"));
                    int b = yyjson_get_int(yyjson_obj_get(j_color, "blue"));
                    double alpha = yyjson_get_num(yyjson_obj_get(j_color, "alpha"));
                    
                    int a = static_cast<int>(std::lerp(0.0, 255.0, std::clamp(alpha, 0.0, 1.0)));

                    wa.color = std::format("{:x}{:x}{:x}{:x}", r, g, b, a);
                }

                wa.expire_time = jsonGetStrValue(j_val, "expireTime").substr(0, 16).replace(10, 1, " ");
                wa.headline = jsonGetStrValue(j_val, "headline");
                wa.description = jsonGetStrValue(j_val, "description");

                return wa;
            };

            auto *j_arr_alerts = yyjson_obj_get(j_val, "alerts");
            size_t idx{ 0 }, max{ 0 };
            yyjson_val *j_alert{ nullptr };
            yyjson_arr_foreach(j_arr_alerts, idx, max, j_alert) {
                rt_alerts.alerts.push_back(extractAlertInfo(j_alert));
            }

            auto *j_arr_attributions = yyjson_obj_get(j_metadata, "attributions");
            idx = max = 0;
            yyjson_val *j_attrib{ nullptr };
            yyjson_arr_foreach(j_arr_attributions, idx, max, j_attrib) {
                rt_alerts.attributions.push_back(yyjson_get_str(j_attrib));
            }
        };

        if (queryFrame(url_host, url_path, cfg, func)) {
            return true;
        } else {
            //Logger::instance().error(L"QueryRealtimeWeather failed");
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_RTWA_FAILED));
            return false;
        }
    }

    static std::string formatAlerts(const DataProviderQWeather::RealtimeWeatherAlerts &rtw_alerts) {
        if (rtw_alerts.alerts.empty()) {
            return std::string{};
        }
        
        std::ostringstream oss;
        for (const auto &alert : rtw_alerts.alerts) {
            oss << alert.headline << std::endl;
            oss << "(" << alert.issued_time << " ~ " << alert.expire_time << ")" << std::endl;
            oss << alert.description << std::endl << std::endl;
        }

        for (const auto &attrib : rtw_alerts.attributions) {
            oss << attrib << std::endl;
        }

        return oss.str();
    }

    static bool queryLocations(const std::string &query, const DataProviderQWeather::Config &cfg, Locations &queried_locations) {
        queried_locations.clear();

        std::string url_host = std::format("https://{}", cfg.api_host);
        std::string url_path = std::format("/geo/v2/city/lookup?location={}", query);

        auto func = [&queried_locations](yyjson_val *j_val) {
            auto *j_arr_locations = yyjson_obj_get(j_val, "location");

            auto extractLocationInfo = [](yyjson_val *j_val) {
                return Location{
                    .id = qw::jsonGetStrValue(j_val, "id"),
                    .name = qw::jsonGetStrValue(j_val, "name"),
                    .administrative_ownership =
                        std::format("{}-{}", qw::jsonGetStrValue(j_val, "adm2"), qw::jsonGetStrValue(j_val, "adm1")),
                    .longitude = qw::jsonGetStrValue(j_val, "lon"),
                    .latitude = qw::jsonGetStrValue(j_val, "lat"),
                };
            };

            size_t idx{ 0 }, max{ 0 };
            yyjson_val *j_loc{ nullptr };
            yyjson_arr_foreach(j_arr_locations, idx, max, j_loc) {
                queried_locations.push_back(extractLocationInfo(j_loc));
            }
        };

        if (qw::queryFrame(url_host, url_path, cfg, func)) {
            return true;
        } else {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_GEOCODING_FAILED));
            return false;
        }
    }
}

bool DataProviderQWeather::validateApiAuthentication() const
{
    if (config_.api_host.empty()) {
        //Logger::instance().error(L"No API host.");
        Logger::instance().error(tr::txt(tr::TextID::ERR_AUTH_NO_API_HOST));
        return false;
    }

    if (config_.enable_jwt) {
        if (config_.project_id.empty()) {
            //Logger::instance().error(L"No project id.");
            Logger::instance().error(tr::txt(tr::TextID::ERR_AUTH_JWT_NO_PROJ_ID));
            return false;
        }

        if (config_.credential_id.empty()) {
            //Logger::instance().error(L"No credential id.");
            Logger::instance().error(tr::txt(tr::TextID::ERR_AUTH_JWT_NO_CRED_ID));
            return false;
        }
    } else {
        if (config_.app_key.empty()) {
            //Logger::instance().error(L"No application key.");
            Logger::instance().error(tr::txt(tr::TextID::ERR_AUTH_NO_APP_KEY));
            return false;
        }
    }

    return true;
}

bool DataProviderQWeather::fetchWeatherData(const Location &loc) {
    if (!validateApiAuthentication()) {
        return false;
    }

    auto all_succeeded{ true };
    all_succeeded &= qw::queryRealtimeWeather(loc, config_, realtime_weather_);
    all_succeeded &= qw::queryRealtimeAirQuality(loc, config_, realtime_air_quality_);
    all_succeeded &= qw::queryForcastedWeather3d(loc, config_, fc_weather_3d_);
    all_succeeded &= qw::queryRealtimeWeatherAlerts(loc, config_, weather_alerts_);

    return all_succeeded;
}

bool DataProviderQWeather::geocodingDirect(const std::string &query, Locations &queried_locations) const {
    if (!validateApiAuthentication()) {
        return false;
    }

    return qw::queryLocations(query, config_, queried_locations);
}

bool DataProviderQWeather::geocodingReverse(const std::string &latitude, const std::string &longitude,
                                            Locations &queried_locations) const {
    if (!validateApiAuthentication()) {
        return false;
    }

    auto query = std::format("{},{}", longitude, latitude);
    return qw::queryLocations(query, config_, queried_locations);
}

std::string DataProviderQWeather::getWeatherContent(WeatherTimeliness wt, WeatherContent wc) const {
    if (wt == WeatherTimeliness::REALTIME) {
        if (wc == WeatherContent::TEMPERATURE) {
            if (config_.show_realtime_temp_feels_like) {
                return std::format("{}℃", realtime_weather_.temp_feels_like);
            } else {
                return std::format("{}℃", realtime_weather_.temp);
            }
        } else if (wc == WeatherContent::WEATHER_TEXT) {
            return realtime_weather_.weather_text;
        } else if (wc == WeatherContent::WEATHER_CODE) {
            return realtime_weather_.weather_code;
        } else if (wc == WeatherContent::HUMIDITY) {
            return std::format("{}%", realtime_weather_.humidity);
        } else if (wc == WeatherContent::WIND) {
            auto wind_direction = 
                std::vformat(tr::txt(tr::TextID::FMT_TMP_WIND_DIRECTION),
                             std::make_format_args(realtime_weather_.wind_direction));
            auto wind_strength = config_.show_realtime_wind_scale ?
                std::vformat(tr::txt(tr::TextID::FMT_TMP_WIND_SCALE),
                             std::make_format_args(realtime_weather_.wind_scale)) :
                std::format("{}km/h", realtime_weather_.wind_speed);
            return std::format("{} {}", realtime_weather_.wind_direction, wind_strength);
        } else if (wc == WeatherContent::AIR_QUALITY) {
            if (!realtime_air_quality_.indexes.empty()) {
                const auto &aqi = realtime_air_quality_.indexes.back();
                return std::format("{}({}: {})", aqi.category, aqi.name, aqi.aqi);
            }
        } else if (wc == WeatherContent::AIR_PM2P5) {
            return realtime_air_quality_.pm2p5;
        } else if (wc == WeatherContent::AIR_PM10) {
            return realtime_air_quality_.pm10;
        } else if (wc == WeatherContent::ALERTS) {
            return qw::formatAlerts(weather_alerts_);
        }
    } else {
        auto getForecastedWeatherContent = [](const ForecastedWeather &fcw, WeatherContent wc) -> std::string {
            if (wc == WeatherContent::TEMPERATURE) {
                return std::format("{}~{}℃", fcw.temp_min, fcw.temp_max);
            } else if (wc == WeatherContent::WEATHER_TEXT) {
                return fcw.weather_day == fcw.weather_night ?
                    fcw.weather_day :
                    std::format("{}~{}", fcw.weather_day, fcw.weather_night);
            } else if (wc == WeatherContent::WEATHER_CODE) {
                return fcw.code_day;
            } else if (wc == WeatherContent::HUMIDITY) {
                return std::format("{}%", fcw.humidity);
            } else if (wc == WeatherContent::UV_INDEX) {
                return fcw.uv_index;
            } else {
                return "";
            }
        };

        if (wt == WeatherTimeliness::TODAY) {
            return getForecastedWeatherContent(fc_weather_3d_[0], wc);
        } else if (wt == WeatherTimeliness::TOMMROW) {
            return getForecastedWeatherContent(fc_weather_3d_[1], wc);
        } else if (wt == WeatherTimeliness::DAY_AFTER_TOMMROW) {
            return getForecastedWeatherContent(fc_weather_3d_[2], wc);
        }
    }

    return "";
}

std::string DataProviderQWeather::getWeatherSummary() const {
    std::ostringstream oss;

    // weather & temprature
    oss << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::WEATHER_TEXT) << " "
        << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::TEMPERATURE) << " "
        << "(" << realtime_weather_.update_time << ")";

    // wind
    oss << " " << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::WIND);

    // humidity
    oss << std::format(" {}: {}", tr::txt(tr::TextID::FMT_HUMIDITY),
                       getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::HUMIDITY));

    // air quality
    oss << std::endl
        << std::format("{}: {}", tr::txt(tr::TextID::FMT_AIR_QUALITY),
                       getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::AIR_QUALITY));
    oss << std::endl;
    oss << "PM2.5: " << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::AIR_PM2P5);
    oss << " PM10: " << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::AIR_PM10);

    oss << std::endl;

    // weather alerts
    for (const auto &alert : weather_alerts_.alerts) {
        oss << std::format("[!] {} ({})", alert.headline, alert.issued_time) << std::endl;
    }

    // forecasted weather
    auto fcw_formatter = [&oss, this](WeatherTimeliness wt, const std::string &str_wt) {
        oss << str_wt << ": " << getWeatherContent(wt, WeatherContent::WEATHER_TEXT)
            << " " << getWeatherContent(wt, WeatherContent::TEMPERATURE);
        oss << " " << tr::txt(tr::TextID::FMT_HUMIDITY) << ": " << getWeatherContent(wt, WeatherContent::HUMIDITY);
        oss << " " << tr::txt(tr::TextID::FMT_UVI) << ": " << getWeatherContent(wt, WeatherContent::UV_INDEX);

        oss << std::endl;
    };

    fcw_formatter(WeatherTimeliness::TODAY, tr::txt(tr::TextID::FMT_TODAY));
    fcw_formatter(WeatherTimeliness::TOMMROW, tr::txt(tr::TextID::FMT_TOMORROW));
    fcw_formatter(WeatherTimeliness::DAY_AFTER_TOMMROW, tr::txt(tr::TextID::FMT_DAT_AFTER_TOMORROW));

    return oss.str();
}
