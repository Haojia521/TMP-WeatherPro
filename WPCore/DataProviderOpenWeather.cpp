#include "DataProviderOpenWeather.h"
#include "Logger.h"
#include "utils.h"
#include "AppLocale.h"

#include <functional>

#include <yyjson.h>

namespace ow
{
    static bool jsonHasObject(yyjson_val *j_val, const char *key) {
        return yyjson_obj_get(j_val, key) != nullptr;
    }

    static int jsonGetInt(yyjson_val *j_val, const char *key) {
        return yyjson_get_int(yyjson_obj_get(j_val, key));
    }

    static int64_t jsonGetInt64(yyjson_val *j_val, const char *key) {
        return yyjson_get_sint(yyjson_obj_get(j_val, key));
    }

    static std::string jsonGetStr(yyjson_val *j_val, const char *key) {
        return yyjson_get_str(yyjson_obj_get(j_val, key));
    }

    static std::string jsonGetNumberAsStr1dp(yyjson_val *j_val, const char *key) {
        auto num = yyjson_get_num(yyjson_obj_get(j_val, key));
        
        // 四舍五入到 1 位小数
        double rounded = std::round(num * 10.0) / 10.0;

        // 判断是否为整数（允许浮点误差）
        if (std::abs(rounded - std::round(rounded)) < 1e-9) {
            return std::format("{:.0f}", rounded);
        } else {
            return std::format("{:.1f}", rounded);
        }
    }

    using UnitType = DataProviderOpenWeather::UnitType;
    using Config = DataProviderOpenWeather::Config;
    using RealtimeWeather = DataProviderOpenWeather::RealtimeWeather;
    using RealtimeAirQuality = DataProviderOpenWeather::RealtimeAirQuality;
    using ForecastedWeather = DataProviderOpenWeather::ForecastedWeather;
    using Alert = DataProviderOpenWeather::WeatherAlert;
    using WeatherAlerts = DataProviderOpenWeather::WeatherAlerts;

    static const char* getUnitName(UnitType ut) {
        switch (ut) {
            default:
            case UnitType::Metric:
                return "metric";
                break;
            case UnitType::Standard:
                return "standard";
                break;
            case UnitType::Imperial:
                return "imperial";
                break;
        }
    }

    static const char* getSpeedUnit(UnitType ut) {
        switch (ut) {
            default:
            case UnitType::Metric:
            case UnitType::Standard:
                return "m/s";
                break;
            case UnitType::Imperial:
                return "mph";
                break;
        }
    }

    static const char* getTemperatureUnit(UnitType ut) {
        switch (ut)
        {
            default:
            case UnitType::Metric:
                return "℃";
                break;
            case UnitType::Standard:
                return "K";
                break;
            case UnitType::Imperial:
                return "℉";
                break;
        }
    }

    static const std::string& getWindDirectionText(int deg) {
        static const std::array<std::string, 8> directions = {
            "N", "NE", "E", "SE", "S", "SW", "W", "NW"
        };

        int normalized = ((deg % 360) + 360) % 360;
        int index = static_cast<int>((normalized + 22.5) / 45.0) % 8;

        return directions[index];
    }

    static bool queryFrame(const std::string &path, std::function<void(yyjson_val*)> func) {
        const std::string host{ "http://api.openweathermap.org" };

        // get response content
        std::string content;
        auto status_code = utils::internetGet(host, path, content);

        auto succeed{ false };
        if (!content.empty()) {
            std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                yyjson_read(content.c_str(), content.size(), 0),
                [](yyjson_doc *p) { yyjson_doc_free(p); }
            );

            if (doc != nullptr) {
                auto *root = yyjson_doc_get_root(doc.get());

                if (status_code == 200) {
                    func(root);
                    succeed = true;
                } else {
                    Logger::instance().error(std::format("[{}] {}", status_code, jsonGetStr(root, "message")));
                }
            } else {
                Logger::instance().error(tr::txt(tr::TextID::ERR_PARSING_JSON_FAILED));
            }
        } else {
            Logger::instance().error(
                std::format("[{}] {}", status_code, tr::txt(tr::TextID::ERR_INTERNET_EMPTY_RESPONSE)));
        }

        return succeed;
    }

    static bool queryRealtimeWeather(const Location &loc, const Config &cfg, RealtimeWeather &rt_weather) {
        rt_weather = RealtimeWeather{};

        std::string url_path = std::format("/data/2.5/weather?lat={}&lon={}&appid={}&units={}&lang={}",
                                           loc.latitude, loc.longitude,
                                           cfg.api_key, getUnitName(cfg.unit_type),
                                           tr::txt(tr::TextID::LC_OPENWEATHER)
        );

        auto func = [&rt_weather, &cfg](yyjson_val *j_root) {
            // weather
            auto *j_weather = yyjson_arr_get(yyjson_obj_get(j_root, "weather"), 0);
            if (j_weather != nullptr) {
                rt_weather.weather_main = jsonGetStr(j_weather, "main");
                rt_weather.weather_description = jsonGetStr(j_weather, "description");
                rt_weather.weather_icon = jsonGetStr(j_weather, "icon");
            }

            // temperature & humidity
            auto *j_main = yyjson_obj_get(j_root, "main");
            if (j_main != nullptr) {
                rt_weather.temp = jsonGetNumberAsStr1dp(j_main, "temp");
                rt_weather.temp_feels_like = jsonGetNumberAsStr1dp(j_main, "feels_like");
                rt_weather.humidity = std::to_string(jsonGetInt(j_main, "humidity"));
            }

            // wind
            auto j_wind = yyjson_obj_get(j_root, "wind");
            if (j_wind != nullptr) {
                rt_weather.wind_speed = jsonGetNumberAsStr1dp(j_wind, "speed");
                rt_weather.wind_direction = getWindDirectionText(jsonGetInt(j_wind, "deg"));
            }

            // precipitation
            if (jsonHasObject(j_root, "rain")) {
                auto *j_precip_rain = yyjson_obj_get(j_root, "rain");
                rt_weather.precipitation_rain_1h = jsonGetNumberAsStr1dp(j_precip_rain, "1h");
            }
            if (jsonHasObject(j_root, "snow")) {
                auto *j_precip_snow = yyjson_obj_get(j_root, "snow");
                rt_weather.precipitation_snow_1h = jsonGetNumberAsStr1dp(j_precip_snow, "1h");
            }

            // update time
            auto ts = jsonGetInt(j_root, "dt") + jsonGetInt(j_root, "timezone");
            rt_weather.update_time = utils::timestamp_string_time(ts);

            rt_weather.unit_type = cfg.unit_type;
        };

        if (queryFrame(url_path, func)) {
            return true;
        } else {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_RTW_FAILED));
            return false;
        }
    }

    static bool queryForecastedWeather(const Location &loc, const Config &cfg,
                                       std::array<ForecastedWeather, 3> &fc_weather_3d) {
        fc_weather_3d = std::array<ForecastedWeather, 3>{};

        std::string url_path = std::format("/data/2.5/forecast/daily?cnt=3&lat={}&lon={}&units={}&appid={}&lang={}",
                                           loc.latitude, loc.longitude, getUnitName(cfg.unit_type), cfg.api_key,
                                           tr::txt(tr::TextID::LC_OPENWEATHER));

        auto func = [&fc_weather_3d, &cfg](yyjson_val *j_root) {
            auto extractFcWeather = [](yyjson_val *j_fc_weahter, ForecastedWeather &fc_weather) {
                // weather
                auto *j_weather = yyjson_arr_get(yyjson_obj_get(j_fc_weahter, "weather"), 0);
                if (j_weather != nullptr) {
                    fc_weather.weather_main = jsonGetStr(j_weather, "main");
                    fc_weather.weather_description = jsonGetStr(j_weather, "description");
                    fc_weather.weather_icon = jsonGetStr(j_weather, "icon");
                }

                // temprature
                auto *j_temp = yyjson_obj_get(j_fc_weahter, "temp");
                if (j_temp != nullptr) {
                    fc_weather.temp_max = jsonGetNumberAsStr1dp(j_temp, "max");
                    fc_weather.temp_min = jsonGetNumberAsStr1dp(j_temp, "min");
                }

                // humidity
                fc_weather.humidity = std::to_string(jsonGetInt(j_fc_weahter, "humidity"));

                // wind
                fc_weather.wind_speed = jsonGetNumberAsStr1dp(j_fc_weahter, "speed");
                fc_weather.wind_direction = getWindDirectionText(jsonGetInt(j_fc_weahter, "deg"));

                // precipitation
                if (jsonHasObject(j_fc_weahter, "rain")) {
                    fc_weather.precipitation_rain = jsonGetNumberAsStr1dp(j_fc_weahter, "rain");
                }
                if (jsonHasObject(j_fc_weahter, "snow")) {
                    fc_weather.precipitation_snow = jsonGetNumberAsStr1dp(j_fc_weahter, "snow");
                }
                {
                    auto prob = yyjson_get_real(yyjson_obj_get(j_fc_weahter, "pop"));
                    fc_weather.precipitation_probability = std::to_string(static_cast<int>(prob * 100));
                }
            };


            yyjson_val *j_arr_fc_weather = yyjson_obj_get(j_root, "list");
            auto max_size = std::min(yyjson_arr_size(j_arr_fc_weather), size_t{ 3 });
            for (size_t i = 0; i < max_size; ++i) {
                extractFcWeather(yyjson_arr_get(j_arr_fc_weather, i), fc_weather_3d[i]);
                fc_weather_3d[i].unit_type = cfg.unit_type;
            }
        };

        if (queryFrame(url_path, func)) {
            return true;
        } else {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_FCW3D_FAILED));
            return false;
        }
    }

    static bool queryRealtimeAirQuality(const Location &loc, const Config &cfg, RealtimeAirQuality &rt_air) {
        rt_air = RealtimeAirQuality{};

        std::string url_path = std::format("/data/2.5/air_pollution?lat={}&lon={}&appid={}",
                                           loc.latitude, loc.longitude, cfg.api_key);

        auto func = [&rt_air](yyjson_val *j_root) {
            auto *j_arr_air = yyjson_obj_get(j_root, "list");
            if (yyjson_arr_size(j_arr_air) > 0) {
                auto *j_air = yyjson_arr_get(j_arr_air, 0);

                auto *j_main = yyjson_obj_get(j_air, "main");
                if (j_main != nullptr) {
                    rt_air.aqi = jsonGetNumberAsStr1dp(j_main, "aqi");
                }

                auto *j_comp = yyjson_obj_get(j_air, "components");
                if (j_comp != nullptr) {
                    rt_air.pm2p5 = jsonGetNumberAsStr1dp(j_comp, "pm2_5");
                    rt_air.pm10 = jsonGetNumberAsStr1dp(j_comp, "pm10");
                }
            }
        };

        if (queryFrame(url_path, func)) {
            return true;
        } else {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_RTAQ_FAILED));
            return false;
        }
    }

    static bool onecall(const Location &loc, const Config &cfg,
                        RealtimeWeather &rt_weathe, std::array<ForecastedWeather, 3> &fc_weather_3d,
                        WeatherAlerts &alerts) {
        rt_weathe = RealtimeWeather{};
        fc_weather_3d = std::array<ForecastedWeather, 3>{};
        alerts = WeatherAlerts{};

        std::string url_path = std::format("/data/3.0/onecall?exclude=minutely,hourly&lat={}&lon={}&apppid={}&units={}&lang={}",
                                           loc.latitude, loc.longitude, cfg.api_key, getUnitName(cfg.unit_type),
                                           tr::txt(tr::TextID::LC_OPENWEATHER));

        auto func = [&rt_weathe, &fc_weather_3d, &alerts](yyjson_val *j_root) {
            auto timezone_offset = jsonGetInt64(j_root, "timezone_offset");

            // current weather
            auto j_currnet = yyjson_obj_get(j_root, "current");
            if (j_currnet != nullptr) {
                // weather
                auto *j_weather = yyjson_obj_get(j_currnet, "weather");
                if (j_weather != nullptr) {
                    rt_weathe.weather_main = jsonGetStr(j_weather, "main");
                    rt_weathe.weather_description = jsonGetStr(j_weather, "description");
                    rt_weathe.weather_icon = jsonGetStr(j_weather, "icon");
                }

                // temprature
                rt_weathe.temp = jsonGetNumberAsStr1dp(j_currnet, "temp");
                rt_weathe.temp_feels_like = jsonGetNumberAsStr1dp(j_currnet, "feels_like");
                
                // humidity & uvi
                rt_weathe.humidity = jsonGetNumberAsStr1dp(j_currnet, "humidity");
                rt_weathe.uv_index = jsonGetNumberAsStr1dp(j_currnet, "uvi");

                // wind
                rt_weathe.wind_speed = jsonGetNumberAsStr1dp(j_currnet, "wind_speed");
                rt_weathe.wind_direction = getWindDirectionText(jsonGetInt(j_currnet, "wind_deg"));

                // precipitation
                if (jsonHasObject(j_currnet, "rain")) {
                    auto *j_rain = yyjson_obj_get(j_currnet, "rain");
                    rt_weathe.precipitation_rain_1h = jsonGetNumberAsStr1dp(j_rain, "1h");
                }
                if (jsonHasObject(j_currnet, "snow")) {
                    auto *j_snow = yyjson_obj_get(j_currnet, "snow");
                    rt_weathe.precipitation_snow_1h = jsonGetNumberAsStr1dp(j_snow, "1h");
                }

                rt_weathe.update_time = utils::timestamp_string_time(jsonGetInt64(j_currnet, "dt") + timezone_offset);
            }

            // forecast weather 3d
            auto extractFcWeather = [](yyjson_val *j_daily_fcw, ForecastedWeather &fc_weather) {
                // weather
                auto *j_weahter = yyjson_obj_get(j_daily_fcw, "weather");
                if (j_weahter != nullptr) {
                    fc_weather.weather_main = jsonGetStr(j_weahter, "main");
                    fc_weather.weather_description = jsonGetStr(j_weahter, "description");
                    fc_weather.weather_icon = jsonGetStr(j_weahter, "icon");
                }

                // temperature
                auto *j_temp = yyjson_obj_get(j_daily_fcw, "temp");
                if (j_temp != nullptr) {
                    fc_weather.temp_max = jsonGetNumberAsStr1dp(j_temp, "max");
                    fc_weather.temp_min = jsonGetNumberAsStr1dp(j_temp, "min");
                }

                // humidity & uvi
                fc_weather.humidity = jsonGetNumberAsStr1dp(j_daily_fcw, "humidity");
                fc_weather.uv_index = jsonGetNumberAsStr1dp(j_daily_fcw, "uvi");

                // wind
                fc_weather.wind_speed = jsonGetNumberAsStr1dp(j_daily_fcw, "wind_speed");
                fc_weather.wind_direction = getWindDirectionText(jsonGetInt(j_daily_fcw, "wind_deg"));

                // precipitation
                if (jsonHasObject(j_daily_fcw, "rain")) {
                    fc_weather.precipitation_rain = jsonGetNumberAsStr1dp(j_daily_fcw, "rain");
                }
                if (jsonHasObject(j_daily_fcw, "snow")) {
                    fc_weather.precipitation_snow = jsonGetNumberAsStr1dp(j_daily_fcw, "snow");
                }
                {
                    auto prob = yyjson_get_real(yyjson_obj_get(j_daily_fcw, "pop"));
                    fc_weather.precipitation_probability = std::to_string(static_cast<int>(prob * 100));
                }
            };

            auto *j_arr_daily_fcw = yyjson_obj_get(j_root, "daily");
            auto max_size = std::min(yyjson_arr_size(j_arr_daily_fcw), size_t{ 3 });
            for (size_t i = 0; i < max_size; ++i) {
                extractFcWeather(yyjson_arr_get(j_arr_daily_fcw, i), fc_weather_3d[i]);
            }

            // alerts
            auto *j_arr_alerts = yyjson_obj_get(j_root, "alerts");
            if (j_arr_alerts != nullptr) {
                auto extractAlert = [timezone_offset](yyjson_val *j_alert) {
                    return Alert{
                        .sender = jsonGetStr(j_alert, "sender_name"),
                        .event = jsonGetStr(j_alert, "event"),
                        .start_datetime =  utils::timestamp_string(jsonGetInt64(j_alert, "start") + timezone_offset),
                        .end_datetime = utils::timestamp_string(jsonGetInt64(j_alert, "end") + timezone_offset),
                        .description = jsonGetStr(j_alert, "description")
                    };
                };

                size_t idx{ 0 }, max{ 0 };
                yyjson_val *j_alert{ nullptr };
                yyjson_arr_foreach(j_arr_alerts, idx, max, j_alert) {
                    alerts.push_back(extractAlert(j_alert));
                }
            }
        };

        if (queryFrame(url_path, func)) {
            return true;
        } else {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_ONECALL_FAILED));
            return false;
        }
    }

    static Location geocodingExtractLocationInfo(yyjson_val *j_loc) {
        Location loc;

        auto *j_local_names = yyjson_obj_get(j_loc, "local_names");
        auto lang_code = tr::txt(tr::TextID::LC_OPENWEATHER).substr(0, 2);
        auto local_name = jsonGetStr(j_local_names, lang_code.c_str());
        if (!local_name.empty()) {
            loc.name = local_name;
        } else {
            loc.name = jsonGetStr(j_loc, "name");
        }

        loc.latitude = std::format("{:.4f}", yyjson_get_real(yyjson_obj_get(j_loc, "lat")));
        loc.longitude = std::format("{:.4f}", yyjson_get_real(yyjson_obj_get(j_loc, "lon")));

        auto country = jsonGetStr(j_loc, "country");
        auto state = jsonGetStr(j_loc, "state");
        if (state.empty()) {
            loc.administrative_ownership = country;
        } else {
            loc.administrative_ownership = std::format("{}-{}", state, country);
        }

        return loc;
    }

    static bool geocodingDirect(const std::string &query, const Config &cfg, Locations queried_locations) {
        queried_locations.clear();

        auto url_path = std::format("/geo/1.0/direct?limit=10&q={}&appid={}", query, cfg.api_key);

        auto func = [&queried_locations](yyjson_val *j_root) {
            size_t idx{ 0 }, max{ 0 };
            yyjson_val *j_location{ nullptr };
            yyjson_arr_foreach(j_root, idx, max, j_location) {
                queried_locations.push_back(geocodingExtractLocationInfo(j_location));
            }
        };

        if (queryFrame(url_path, func)) {
            return true;
        } else {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_GEOCODING_FAILED));
            return false;
        }
    }

    static bool geocodingReverse(const std::string &latitude, const std::string &longitude, const Config &cfg,
                                 Locations queried_locations) {
        queried_locations.clear();

        auto url_path = std::format("/geo/1.0/reverse?lat={}&lon={}&limit=10&appid={}",
                                    latitude, longitude, cfg.api_key);

        auto func = [&queried_locations](yyjson_val *j_root) {
            size_t idx{ 0 }, max{ 0 };
            yyjson_val *j_location{ nullptr };
            yyjson_arr_foreach(j_root, idx, max, j_location) {
                queried_locations.push_back(geocodingExtractLocationInfo(j_location));
            }
        };

        if (queryFrame(url_path, func)) {
            return true;
        } else {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_GEOCODING_FAILED));
            return false;
        }
    }

    static std::string formatAlerts(const WeatherAlerts &rt_alerts) {
        if (rt_alerts.empty()) {
            return std::string{};
        }

        std::ostringstream oss;
        for (const auto &alert : rt_alerts) {
            oss << alert.event << std::endl;
            oss << alert.sender << std::endl;
            oss << "(" << alert.start_datetime << " ~ " << alert.end_datetime << ")" << std::endl;
            oss << alert.description << std::endl << std::endl;
        }

        return oss.str();
    }
}

bool DataProviderOpenWeather::validateApiAuthentication() const {
    if (config_.api_key.empty()) {
        Logger::instance().error(tr::txt(tr::TextID::ERR_AUTH_NO_APP_KEY));
        return false;
    }

    return true;
}

bool DataProviderOpenWeather::geocodingDirect(const std::string &query, Locations &queried_locations) const {
    if (!validateApiAuthentication()) {
        return false;
    }

    return ow::geocodingDirect(query, config_, queried_locations);
}

bool DataProviderOpenWeather::geocodingReverse(const std::string &latitude, const std::string &longitude,
                                               Locations &queried_locations) const {
    if (!validateApiAuthentication()) {
        return false;
    }

    return ow::geocodingReverse(latitude, longitude, config_, queried_locations);
}

bool DataProviderOpenWeather::fetchWeatherData(const Location &loc) {
    if (!validateApiAuthentication()) {
        return false;
    }

    auto succeed{ true };
    if (config_.use_onecall) {
        succeed &= ow::onecall(loc, config_, realtime_weather_, fc_weather_3d_, alerts_);
    } else {
        succeed &= ow::queryRealtimeWeather(loc, config_, realtime_weather_);
        succeed &= ow::queryForecastedWeather(loc, config_, fc_weather_3d_);

        alerts_.clear();
    }
    succeed &= ow::queryRealtimeAirQuality(loc, config_, realtime_air_);

    return succeed;
}

std::string DataProviderOpenWeather::getWeatherContent(WeatherTimeliness wt, WeatherContent wc) const {
    if (wt == WeatherTimeliness::REALTIME) {
        if (wc == WeatherContent::TEMPERATURE) {
            return std::format("{}{}", realtime_weather_.temp, ow::getTemperatureUnit(realtime_weather_.unit_type));
        } else if (wc == WeatherContent::WEATHER_TEXT) {
            return realtime_weather_.weather_description;
        } else if (wc == WeatherContent::WEATHER_CODE) {
            return realtime_weather_.weather_icon;
        } else if (wc == WeatherContent::HUMIDITY) {
            return std::format("{}%", realtime_weather_.humidity);
        } else if (wc == WeatherContent::WIND) {
            return std::format("{} {}{}", realtime_weather_.wind_direction,
                               realtime_weather_.wind_speed, ow::getSpeedUnit(realtime_weather_.unit_type));
        } else if (wc == WeatherContent::UV_INDEX) {
            return realtime_weather_.uv_index;
        } else if (wc == WeatherContent::PRECIPITATION) {
            if (!realtime_weather_.precipitation_rain_1h.empty()) {
                return std::format("rain {}mm", realtime_weather_.precipitation_rain_1h);
            } else if (!realtime_weather_.precipitation_snow_1h.empty()) {
                return std::format("snow {}mm", realtime_weather_.precipitation_snow_1h);
            } else {
                return "";
            }
        } else if (wc == WeatherContent::AIR_QUALITY) {
            return realtime_air_.aqi;
        } else if (wc == WeatherContent::AIR_PM2P5) {
            return realtime_air_.pm2p5;
        } else if (wc == WeatherContent::AIR_PM10) {
            return realtime_air_.pm10;
        } else if (wc == WeatherContent::ALERTS) {
            return ow::formatAlerts(alerts_);
        }
    } else {
        auto getForecastedWeatherContent = [](const ForecastedWeather &fcw, WeatherContent wc) -> std::string {
            if (wc == WeatherContent::TEMPERATURE) {
                return std::format("{}~{}{}", fcw.temp_min, fcw.temp_max, ow::getTemperatureUnit(fcw.unit_type));
            } else if (wc == WeatherContent::WEATHER_TEXT) {
                return fcw.weather_description;
            } else if (wc == WeatherContent::WEATHER_CODE) {
                return fcw.weather_icon;
            } else if (wc == WeatherContent::HUMIDITY) {
                return std::format("{}%", fcw.humidity);
            } else if (wc == WeatherContent::UV_INDEX) {
                return fcw.uv_index;
            } else if (wc == WeatherContent::WIND) {
                return std::format("{} {}{}", fcw.wind_direction, fcw.wind_speed, ow::getSpeedUnit(fcw.unit_type));
            } else if (wc == WeatherContent::PRECIPITATION) {
                if (!fcw.precipitation_rain.empty()) {
                    return std::format("{}% rain {}mm", fcw.precipitation_probability, fcw.precipitation_rain);
                } else if (!fcw.precipitation_snow.empty()) {
                    return std::format("{}% snow {}mm", fcw.precipitation_probability, fcw.precipitation_rain);
                } else {
                    return std::format("{}%", fcw.precipitation_probability);
                }
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

std::string DataProviderOpenWeather::getWeatherSummary() const {
    std::ostringstream oss;

    // weather and temperature
    oss << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::WEATHER_TEXT) << " "
        << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::TEMPERATURE) << " "
        << "(" << realtime_weather_.update_time << ")";

    // wind
    oss << " " << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::WIND);

    // humidity
    oss << std::format(" {}: {}", tr::txt(tr::TextID::FMT_HUMIDITY),
                       getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::HUMIDITY));

    // uv index
    oss << std::format(" {}: {}", tr::txt(tr::TextID::FMT_UVI),
                       getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::UV_INDEX));

    // air quality
    oss << std::endl
        << "AQI: " << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::AIR_QUALITY)
        << " PM2.5: " << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::AIR_PM2P5)
        << " PM10: " << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::AIR_PM10);

    // weather alerts
    oss << std::endl;
    for (const auto &alt : alerts_) {
        oss << "[!] " << alt.event << std::endl;
    }

    // forecast weather
    auto fcw_formatter = [&oss, this](WeatherTimeliness wt, const std::string &str_wt) {
        // the forecast weater may be empty
        if (getWeatherContent(wt, WeatherContent::WEATHER_TEXT).empty()) {
            return;
        }

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
