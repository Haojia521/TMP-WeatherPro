#include "DataProviderOpenWeather.h"
#include "Logger.h"
#include "utils.h"
#include "AppLocale.h"

#include <functional>
#include <future>

#include <yyjson.h>

namespace
{
    bool jsonHasObject(yyjson_val *j_val, const char *key) {
        return yyjson_obj_get(j_val, key) != nullptr;
    }

    int jsonGetInt(yyjson_val *j_val, const char *key) {
        return yyjson_get_int(yyjson_obj_get(j_val, key));
    }

    int64_t jsonGetInt64(yyjson_val *j_val, const char *key) {
        return yyjson_get_sint(yyjson_obj_get(j_val, key));
    }

    std::string jsonGetStr(yyjson_val *j_val, const char *key) {
        return yyjson_get_str(yyjson_obj_get(j_val, key));
    }

    std::string jsonGetNumberAsStr1dp(yyjson_val *j_val, const char *key) {
        const auto num = yyjson_get_num(yyjson_obj_get(j_val, key));
        
        // 四舍五入到 1 位小数，如果小数部分为0则只返回整数部分
        if (auto rounded = std::round(num * 10.0) / 10.0; 
            std::abs(rounded - std::round(rounded)) < 1e-9) {
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

    const char* getUnitName(UnitType ut) {
        switch (ut) {
            case UnitType::Metric:
                return "metric";

            case UnitType::Standard:
                return "standard";

            case UnitType::Imperial:
                return "imperial";
        }

        return "metric";
    }

    const char* getSpeedUnit(UnitType ut) {
        switch (ut) {
            case UnitType::Metric:
            case UnitType::Standard:
                return "m/s";

            case UnitType::Imperial:
                return "mph";
        }

        return "m/s";
    }

    const char* getTemperatureUnit(UnitType ut) {
        switch (ut)
        {
            case UnitType::Metric:
                return "℃";

            case UnitType::Standard:
                return "K";

            case UnitType::Imperial:
                return "℉";
        }

        return "℃";
    }

    const std::string& getWindDirectionText(int deg) {
        const static std::array<std::string, 8> directions = {
            "N", "NE", "E", "SE", "S", "SW", "W", "NW"
        };

        const int normalized = ((deg % 360) + 360) % 360;
        const int index = static_cast<int>((normalized + 22.5) / 45.0) % 8;

        return directions[index];
    }

    bool queryFrame(const std::string &path, utils::HttpParams &params, const std::string &app_id,
                    const std::function<void(yyjson_val*)> &func) {
        constexpr std::string_view HOST{ "http://api.openweathermap.org" };

        params.data.emplace("appid", app_id);

        // get response content
        std::string content;

        auto status_code = utils::internetGetWithRetry(std::string{ HOST }, path, content, params);

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
                Logger::instance().error(tr::txt(tr::TID::ERR_PARSING_JSON_FAILED));
            }
        } else {
            Logger::instance().error(
                std::format("[{}] {}", status_code, tr::txt(tr::TID::ERR_INTERNET_EMPTY_RESPONSE)));
        }

        return succeed;
    }

    bool queryWeatherFrame(const Location &loc, const std::string &path, utils::HttpParams &params, const std::string &app_id,
                    const std::function<void(yyjson_val*)> &func) {
        constexpr std::string_view HOST{ "http://api.openweathermap.org" };

        if (loc.longitude.empty() || loc.latitude.empty()) {
            Logger::instance().error(tr::txt(tr::TID::ERR_NO_LONG_LAT));
            return false;
        }

        params.data.emplace("lat", loc.latitude);
        params.data.emplace("lon", loc.longitude);
        params.data.emplace("lang", tr::txt(tr::TID::LC_OPENWEATHER));

        return queryFrame(path, params, app_id, func);
    }

    RealtimeWeather queryRealtimeWeather(const Location &loc, const Config &cfg) {
        RealtimeWeather rt_weather{};

        const std::string url_path{ "/data/2.5/weather" };

        utils::HttpParams params;
        params.data.emplace("units", getUnitName(cfg.unit_type));

        auto func = [&rt_weather, &cfg](yyjson_val *j_root) {
            // weather
            if (auto *j_weather = yyjson_arr_get(yyjson_obj_get(j_root, "weather"), 0); 
                j_weather != nullptr) {
                rt_weather.weather_main = jsonGetStr(j_weather, "main");
                rt_weather.weather_description = jsonGetStr(j_weather, "description");
                rt_weather.weather_icon = jsonGetStr(j_weather, "icon");
            }

            // temperature & humidity
            if (auto *j_main = yyjson_obj_get(j_root, "main"); 
                j_main != nullptr) {
                rt_weather.temp = jsonGetNumberAsStr1dp(j_main, "temp");
                rt_weather.temp_feels_like = jsonGetNumberAsStr1dp(j_main, "feels_like");
                rt_weather.humidity = std::to_string(jsonGetInt(j_main, "humidity"));
            }

            // wind
            if (auto *j_wind = yyjson_obj_get(j_root, "wind"); 
                j_wind != nullptr) {
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
            const auto ts = jsonGetInt(j_root, "dt") + jsonGetInt(j_root, "timezone");
            rt_weather.update_time = utils::timestamp_string_time(ts);

            rt_weather.unit_type = cfg.unit_type;
        };

        if (!queryWeatherFrame(loc, url_path, params, cfg.api_key, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_RTW_FAILED));
        }

        return rt_weather;
    }

    std::array<ForecastedWeather, 3> queryForecastedWeather(const Location &loc, const Config &cfg) {
        std::array<ForecastedWeather, 3> fc_weather_3d{};

        const std::string url_path{ "/data/2.5/forecast/daily" };

        utils::HttpParams params;
        params.data.emplace("cnt", "3");
        params.data.emplace("units", getUnitName(cfg.unit_type));

        auto func = [&fc_weather_3d, &cfg](yyjson_val *j_root) {
            auto extractFcWeather = [](yyjson_val *j_fc_weahter, ForecastedWeather &fc_weather) {
                // weather
                if (auto *j_weather = yyjson_arr_get(yyjson_obj_get(j_fc_weahter, "weather"), 0); 
                    j_weather != nullptr) {
                    fc_weather.weather_main = jsonGetStr(j_weather, "main");
                    fc_weather.weather_description = jsonGetStr(j_weather, "description");
                    fc_weather.weather_icon = jsonGetStr(j_weather, "icon");
                }

                // temprature
                if (auto *j_temp = yyjson_obj_get(j_fc_weahter, "temp"); 
                    j_temp != nullptr) {
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
                    const auto prob = yyjson_get_real(yyjson_obj_get(j_fc_weahter, "pop"));
                    fc_weather.precipitation_probability = std::to_string(static_cast<int>(prob * 100));
                }
            };


            yyjson_val *j_arr_fc_weather = yyjson_obj_get(j_root, "list");
            const auto max_size = std::min(yyjson_arr_size(j_arr_fc_weather), size_t{ 3 });
            for (size_t i = 0; i < max_size; ++i) {
                extractFcWeather(yyjson_arr_get(j_arr_fc_weather, i), fc_weather_3d[i]);
                fc_weather_3d[i].unit_type = cfg.unit_type;
            }
        };

        if (!queryWeatherFrame(loc, url_path, params, cfg.api_key, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_FCW3D_FAILED));
        }

        return fc_weather_3d;
    }

    RealtimeAirQuality queryRealtimeAirQuality(const Location &loc, const Config &cfg) {
        RealtimeAirQuality rt_air{};

        const std::string url_path{ "/data/2.5/air_pollution" };

        utils::HttpParams params{};

        auto func = [&rt_air](yyjson_val *j_root) {
            auto *j_arr_air = yyjson_obj_get(j_root, "list");
            if (yyjson_arr_size(j_arr_air) > 0) {
                auto *j_air = yyjson_arr_get(j_arr_air, 0);

                if (auto *j_main = yyjson_obj_get(j_air, "main"); 
                    j_main != nullptr) {
                    rt_air.aqi = jsonGetNumberAsStr1dp(j_main, "aqi");
                }

                if (auto *j_comp = yyjson_obj_get(j_air, "components"); 
                    j_comp != nullptr) {
                    rt_air.pm2p5 = jsonGetNumberAsStr1dp(j_comp, "pm2_5");
                    rt_air.pm10 = jsonGetNumberAsStr1dp(j_comp, "pm10");
                }
            }
        };

        if (!queryWeatherFrame(loc, url_path, params, cfg.api_key, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_RTAQ_FAILED));
        }

        return rt_air;
    }

    auto onecall(const Location &loc, const Config &cfg) {
        RealtimeWeather rt_weathe{};
        std::array<ForecastedWeather, 3> fc_weather_3d{};
        WeatherAlerts alerts{};

        const std::string url_path{ "/data/3.0/onecall" };

        utils::HttpParams params;
        params.data.emplace("exclude", "minutely,hourly");
        params.data.emplace("units", getUnitName(cfg.unit_type));

        auto func = [&rt_weathe, &fc_weather_3d, &alerts](yyjson_val *j_root) {
            auto timezone_offset = jsonGetInt64(j_root, "timezone_offset");

            // current weather
            
            if (auto j_currnet = yyjson_obj_get(j_root, "current");  j_currnet != nullptr) {
                // weather
                if (auto *j_weather = yyjson_obj_get(j_currnet, "weather"); 
                    j_weather != nullptr) {
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
                if (auto *j_weahter = yyjson_obj_get(j_daily_fcw, "weather"); j_weahter != nullptr) {
                    fc_weather.weather_main = jsonGetStr(j_weahter, "main");
                    fc_weather.weather_description = jsonGetStr(j_weahter, "description");
                    fc_weather.weather_icon = jsonGetStr(j_weahter, "icon");
                }

                // temperature
                if (auto *j_temp = yyjson_obj_get(j_daily_fcw, "temp"); j_temp != nullptr) {
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
            const auto max_size = std::min(yyjson_arr_size(j_arr_daily_fcw), size_t{ 3 });
            for (size_t i = 0; i < max_size; ++i) {
                extractFcWeather(yyjson_arr_get(j_arr_daily_fcw, i), fc_weather_3d[i]);
            }

            // alerts
            if (auto *j_arr_alerts = yyjson_obj_get(j_root, "alerts"); j_arr_alerts != nullptr) {
                auto extractAlert = [timezone_offset](yyjson_val *j_alert) {
                    return Alert{
                        .sender = jsonGetStr(j_alert, "sender_name"),
                        .event = jsonGetStr(j_alert, "event"),
                        .start_datetime =  utils::timestamp_string(jsonGetInt64(j_alert, "start") + timezone_offset),
                        .end_datetime = utils::timestamp_string(jsonGetInt64(j_alert, "end") + timezone_offset),
                        .description = jsonGetStr(j_alert, "description")
                    };
                };

                size_t idx, max;
                yyjson_val *j_alert;
                yyjson_arr_foreach(j_arr_alerts, idx, max, j_alert) {
                    alerts.push_back(extractAlert(j_alert));
                }
            }
        };

        if (!queryWeatherFrame(loc, url_path, params, cfg.api_key, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_ONECALL_FAILED));
        }

        return std::make_tuple(rt_weathe, fc_weather_3d, alerts);
    }

    Location geocodingExtractLocationInfo(yyjson_val *j_loc) {
        Location loc;

        auto *j_local_names = yyjson_obj_get(j_loc, "local_names");
        const std::string lang_code{ tr::txt(tr::TID::LC_OPENWEATHER).substr(0, 2) };
        if (const auto local_name = jsonGetStr(j_local_names, lang_code.c_str());
            !local_name.empty()) {
            loc.name = local_name;
        } else {
            loc.name = jsonGetStr(j_loc, "name");
        }

        loc.latitude = std::format("{:.4f}", yyjson_get_real(yyjson_obj_get(j_loc, "lat")));
        loc.longitude = std::format("{:.4f}", yyjson_get_real(yyjson_obj_get(j_loc, "lon")));

        auto country = jsonGetStr(j_loc, "country");
        if (auto state = jsonGetStr(j_loc, "state"); 
            state.empty()) {
            loc.administrative_ownership = country;
        } else {
            loc.administrative_ownership = std::format("{}-{}", state, country);
        }

        return loc;
    }

    bool geocodingDirect(const std::string &query, const Config &cfg, Locations &queried_locations) {
        queried_locations.clear();

        const auto url_path{ "/geo/1.0/direct" };

        utils::HttpParams params;
        params.data.emplace("limit", "10");
        params.data.emplace("q", query);

        auto func = [&queried_locations](yyjson_val *j_root) {
            size_t idx, max;
            yyjson_val *j_location;
            yyjson_arr_foreach(j_root, idx, max, j_location) {
                queried_locations.push_back(geocodingExtractLocationInfo(j_location));
            }
        };

        if (queryFrame(url_path, params, cfg.api_key, func)) {
            return true;
        }

        Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_GEOCODING_FAILED));
        return false;
    }

    bool geocodingReverse(const std::string &latitude, const std::string &longitude, const Config &cfg,
                                 Locations &queried_locations) {
        queried_locations.clear();

        const auto url_path{ "/geo/1.0/reverse" };

        utils::HttpParams params;
        params.data.emplace("limit", "10");
        params.data.emplace("lat", latitude);
        params.data.emplace("lon", longitude);

        auto func = [&queried_locations](yyjson_val *j_root) {
            size_t idx, max;
            yyjson_val *j_location;
            yyjson_arr_foreach(j_root, idx, max, j_location) {
                queried_locations.push_back(geocodingExtractLocationInfo(j_location));
            }
        };

        if (queryFrame(url_path, params, cfg.api_key, func)) {
            // Replace the returned coordinates (usually only one for lat/lng lookup) with the function's 
            // input values to accurately record the specified location
            if (!queried_locations.empty()) {
                queried_locations[0].longitude = longitude;
                queried_locations[0].latitude = latitude;
            }
            return true;
        }

        Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_GEOCODING_FAILED));
        return false;
    }

    std::string formatAlerts(const WeatherAlerts &rt_alerts) {
        if (rt_alerts.empty()) {
            return std::string{};
        }

        std::ostringstream oss;
        for (const auto &alert : rt_alerts) {
            oss << alert.event << "\n";
            oss << alert.sender << "\n";
            oss << "(" << alert.start_datetime << " ~ " << alert.end_datetime << ")" << "\n";
            oss << alert.description << "\n" << "\n";
        }

        return oss.str();
    }
}

std::string DataProviderOpenWeather::WeatherDataBlock::getWeatherSummary() const {
    std::ostringstream oss;

    // weather and temperature
    oss << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::WEATHER_TEXT) << " "
        << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::TEMPERATURE) << " "
        << "(" << realtime_weather.update_time << ")";

    // wind
    oss << "\n" << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::WIND);

    // humidity
    oss << std::format(" {}: {}", tr::txt(tr::TID::FMT_HUMIDITY),
                       getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::HUMIDITY));

    // uv index
    oss << std::format(" {}: {}", tr::txt(tr::TID::FMT_UVI),
                       getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::UV_INDEX));

    // air quality
    oss << "\n"
        << "AQI: " << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::AIR_QUALITY)
        << " PM2.5: " << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::AIR_PM2P5)
        << " PM10: " << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::AIR_PM10);

    // weather alerts
    oss << "\n";
    for (const auto &alt : alerts) {
        oss << "[!] " << alt.event << "\n";
    }

    // forecast weather
    auto fcw_formatter = [&oss, this](WeatherTimeSlot wt, std::string_view str_wt) {
        // the forecast weater may be empty
        if (getWeatherItem(wt, WeatherItem::WEATHER_TEXT).empty()) {
            return;
        }

        oss << "\n" << str_wt << ": " << getWeatherItem(wt, WeatherItem::WEATHER_TEXT)
            << " " << getWeatherItem(wt, WeatherItem::TEMPERATURE);
        oss << " " << tr::txt(tr::TID::FMT_HUMIDITY) << ": " << getWeatherItem(wt, WeatherItem::HUMIDITY);
        oss << " " << tr::txt(tr::TID::FMT_UVI) << ": " << getWeatherItem(wt, WeatherItem::UV_INDEX);
    };

    fcw_formatter(WeatherTimeSlot::TODAY, tr::txt(tr::TID::FMT_TODAY));
    fcw_formatter(WeatherTimeSlot::TOMMROW, tr::txt(tr::TID::FMT_TOMORROW));
    fcw_formatter(WeatherTimeSlot::DAY_AFTER_TOMMROW, tr::txt(tr::TID::FMT_DAT_AFTER_TOMORROW));

    oss << "\n";

    return oss.str();
}

std::string DataProviderOpenWeather::WeatherDataBlock::getWeatherItem(WeatherTimeSlot time_slot, WeatherItem item) const {
    if (time_slot == WeatherTimeSlot::REALTIME) {
        if (item == WeatherItem::TEMPERATURE) {
            return std::format("{}{}", realtime_weather.temp, getTemperatureUnit(realtime_weather.unit_type));
        } else if (item == WeatherItem::WEATHER_TEXT) {
            return realtime_weather.weather_description;
        } else if (item == WeatherItem::WEATHER_CODE) {
            return realtime_weather.weather_icon;
        } else if (item == WeatherItem::HUMIDITY) {
            return std::format("{}%", realtime_weather.humidity);
        } else if (item == WeatherItem::WIND) {
            return std::format("{} {}{}", realtime_weather.wind_direction,
                               realtime_weather.wind_speed, getSpeedUnit(realtime_weather.unit_type));
        } else if (item == WeatherItem::UV_INDEX) {
            return realtime_weather.uv_index;
        } else if (item == WeatherItem::PRECIPITATION) {
            if (!realtime_weather.precipitation_rain_1h.empty()) {
                return std::format("rain {}mm/h", realtime_weather.precipitation_rain_1h);
            } else if (!realtime_weather.precipitation_snow_1h.empty()) {
                return std::format("snow {}mm/h", realtime_weather.precipitation_snow_1h);
            }
        } else if (item == WeatherItem::AIR_QUALITY) {
            return realtime_air.aqi;
        } else if (item == WeatherItem::AIR_PM2P5) {
            return std::format("{}μg/m3", realtime_air.pm2p5);
        } else if (item == WeatherItem::AIR_PM10) {
            return std::format("{}μg/m3", realtime_air.pm10);
        } else if (item == WeatherItem::ALERTS) {
            return formatAlerts(alerts);
        }
    } else {
        auto getForecastedWeatherContent = [](const ForecastedWeather &fcw, WeatherItem item) -> std::string {
            if (item == WeatherItem::TEMPERATURE) {
                return std::format("{}~{}{}", fcw.temp_min, fcw.temp_max, getTemperatureUnit(fcw.unit_type));
            } else if (item == WeatherItem::WEATHER_TEXT) {
                return fcw.weather_description;
            } else if (item == WeatherItem::WEATHER_CODE) {
                return fcw.weather_icon;
            } else if (item == WeatherItem::HUMIDITY) {
                return std::format("{}%", fcw.humidity);
            } else if (item == WeatherItem::UV_INDEX) {
                return fcw.uv_index;
            } else if (item == WeatherItem::WIND) {
                return std::format("{} {}{}", fcw.wind_direction, fcw.wind_speed, getSpeedUnit(fcw.unit_type));
            } else if (item == WeatherItem::PRECIPITATION) {
                if (!fcw.precipitation_rain.empty()) {
                    return std::format("{}% rain {}mm", fcw.precipitation_probability, fcw.precipitation_rain);
                } else if (!fcw.precipitation_snow.empty()) {
                    return std::format("{}% snow {}mm", fcw.precipitation_probability, fcw.precipitation_rain);
                } else {
                    return std::format("{}%", fcw.precipitation_probability);
                }
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

bool DataProviderOpenWeather::validateApiAuthentication() const {
    if (config.api_key.empty()) {
        Logger::instance().error(tr::txt(tr::TID::ERR_AUTH_NO_APP_KEY));
        return false;
    }

    return true;
}

bool DataProviderOpenWeather::geocodingDirect(const std::string &query, Locations &queried_locations) const {
    if (!validateApiAuthentication()) {
        return false;
    }

    return ::geocodingDirect(query, config, queried_locations);
}

bool DataProviderOpenWeather::geocodingReverse(const std::string &latitude, const std::string &longitude,
                                               Locations &queried_locations) const {
    if (!validateApiAuthentication()) {
        return false;
    }

    return ::geocodingReverse(latitude, longitude, config, queried_locations);
}

WeatherDataCPtr DataProviderOpenWeather::getWeatherData(const Location &loc) const {
    if (!validateApiAuthentication()) {
        return nullptr;
    }

    const auto data_block = std::make_shared<WeatherDataBlock>();
    
    if (config.use_onecall) {
        auto fut_onecall = std::async(std::launch::async, [this, &loc] {
            return onecall(loc, config);
        });

        auto [rt_weather, fc_weather_3d, alerts] = fut_onecall.get();
        data_block->realtime_weather = std::move(rt_weather);
        data_block->fc_weather_3d = std::move(fc_weather_3d);
        data_block->alerts = std::move(alerts);
    } else {
        auto fut_rt_weather = std::async(std::launch::async, [this, &loc] {
            return queryRealtimeWeather(loc, config);
        });

        auto fut_fc_weather_3d = std::async(std::launch::async, [this, &loc] {
            return queryForecastedWeather(loc, config);
        });

        data_block->realtime_weather = fut_rt_weather.get();
        data_block->fc_weather_3d = fut_fc_weather_3d.get();
    }

    auto fut_rt_air = std::async(std::launch::async, [this, &loc] {
        return queryRealtimeAirQuality(loc, config);
    });

    data_block->realtime_air = fut_rt_air.get();

    return data_block;
}
