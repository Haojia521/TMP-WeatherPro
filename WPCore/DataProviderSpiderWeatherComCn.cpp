#include "DataProviderSpiderWeatherComCn.h"
#include "AppLocale.h"
#include "Logger.h"
#include "utils.h"
#include "JsonValueHelper.h"

#include <functional>
#include <regex>
#include <ranges>
#include <sstream>

#include <yyjson.h>

namespace
{
    // weather text-code mapping
    struct WeatherTextCodePair
    {
        std::string_view text;
        std::string_view code;
    };

    constexpr std::array<WeatherTextCodePair, 43> WTC_MAPPING_TABLE = { {
        {.text = "晴", .code = "d00"},
        {.text = "多云", .code = "d01"},
        {.text = "阴", .code = "d02"},
        {.text = "阵雨", .code = "d03"},
        {.text = "雷阵雨", .code = "d04"},
        {.text = "雷阵雨伴有冰雹", .code = "d05"},
        {.text = "雨夹雪", .code = "d06"},
        {.text = "小雨", .code = "d07"},
        {.text = "中雨", .code = "d08"},
        {.text = "大雨", .code = "d09"},
        {.text = "暴雨", .code = "d10"},
        {.text = "大暴雨", .code = "d11"},
        {.text = "特大暴雨", .code = "d12"},
        {.text = "阵雪", .code = "d13"},
        {.text = "小雪", .code = "d14"},
        {.text = "中雪", .code = "d15"},
        {.text = "大雪", .code = "d16"},
        {.text = "暴雪", .code = "d17"},
        {.text = "雾", .code = "d18"},
        {.text = "冻雨", .code = "d19"},
        {.text = "沙尘暴", .code = "d20"},
        {.text = "小到中雨", .code = "d21"},
        {.text = "中到大雨", .code = "d22"},
        {.text = "大到暴雨", .code = "d23"},
        {.text = "暴雨到大暴雨", .code = "d24"},
        {.text = "大暴雨到特大暴雨", .code = "d25"},
        {.text = "小到中雪", .code = "d26"},
        {.text = "中到大雪", .code = "d27"},
        {.text = "大到暴雪", .code = "d28"},
        {.text = "浮尘", .code = "d29"},
        {.text = "扬沙", .code = "d30"},
        {.text = "强沙尘暴", .code = "d31"},
        {.text = "霾", .code = "d53"},
        {.text = "无", .code = "d99"},
        {.text = "浓雾", .code = "d32"},
        {.text = "强浓雾", .code = "d49"},
        {.text = "中度霾", .code = "d54"},
        {.text = "重度霾", .code = "d55"},
        {.text = "严重霾", .code = "d56"},
        {.text = "大雾", .code = "d57"},
        {.text = "特强浓雾", .code = "d58"},
        {.text = "雨", .code = "d97"},
        {.text = "雪", .code = "d98"},
    } };

    constexpr std::string_view convWeatherTextToCode(std::string_view text) {
        const auto itr = std::ranges::find_if(
            WTC_MAPPING_TABLE,
            [text](const WeatherTextCodePair &p) { return p.text == text; }
        );

        return itr != WTC_MAPPING_TABLE.end() ? itr->code : "d99";
    }

    //constexpr std::string_view convWeatherCodeToText(std::string_view code) {
    //    auto itr = std::ranges::find_if(
    //        MAPPING_TABLE,
    //        [code](const TcPair &p) { return p.code == code; }
    //    );

    //    return itr != MAPPING_TABLE.end() ? itr->text : "无";
    //}

    using RegexMatch = std::vector<std::string>;
    using RegexMatches = std::vector<RegexMatch>;

    RegexMatches regexExtractAllMatches(const std::string &text, const std::regex &pattern) {
        RegexMatches results;

        for (std::sregex_iterator it(text.begin(), text.end(), pattern),
             end;
             it != end; ++it)
        {
            const std::smatch& matches = *it;

            RegexMatch sub_matches;
            sub_matches.reserve(matches.size());

            for (const auto& sm : matches)
                sub_matches.push_back(sm.str());

            results.push_back(std::move(sub_matches));
        }

        return results;
    }

    constexpr std::string_view AGENT{ "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36 Edg/143.0.0.0" };

    bool queryFrame(std::string_view host, std::string_view path, const utils::HttpParams &params, utils::HttpHeaders &headers,
                    const std::function<bool(const std::string &)> &func) {
        std::string content;

        headers.data.emplace("user-agent", AGENT);

        auto status_code = utils::internetGetWithRetry(std::string{ host }, std::string{ path }, content, params, headers);

        if (!content.empty()) {
            return func(content);
        }

        Logger::instance().error(std::format("[{}] {}", status_code, tr::txt(tr::TID::ERR_INTERNET_EMPTY_RESPONSE)));
        return false;
    }

    using RealtimeWeather = DataProviderSpiderWeatherComCn::RealtimeWeather;
    using ForecastedWeather = DataProviderSpiderWeatherComCn::ForecastedWeather;
    using WeatherAlert = DataProviderSpiderWeatherComCn::WeatherAlert;
    using WeatherAlerts = DataProviderSpiderWeatherComCn::WeatherAlerts;

    auto queryRealtimeWeatherCity(const std::string &code) {
        RealtimeWeather rt_weather{};

        constexpr std::string_view url_host = "https://d1.weather.com.cn";
        const std::string url_path = std::format("/sk_2d/{}.html", code);

        utils::HttpParams params;
        params.data.emplace("_", std::to_string(std::time(nullptr)));

        utils::HttpHeaders headers;
        headers.data.emplace("host", "d1.weather.com.cn");
        headers.data.emplace("referer", "https://www.weather.com.cn/");

        auto func = [&rt_weather](const std::string &response_content) {
            // remove "var dataSK = " from content
            auto content = response_content.substr(response_content.find('{'));

            const std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                yyjson_read(content.c_str(), content.size(), 0),
                [](yyjson_doc *p) { yyjson_doc_free(p); }
            );

            if (doc != nullptr) {
                auto *root = yyjson_doc_get_root(doc.get());

                rt_weather.temp = jvh::getString(root, "temp");
                rt_weather.weather_text = jvh::getString(root, "weather");
                rt_weather.weather_code = jvh::getString(root, "weathercode");
                rt_weather.wind_direction = jvh::getString(root, "WD");
                rt_weather.wind_strength = jvh::getString(root, "WS");
                rt_weather.wind_speed = jvh::getString(root, "wse");
                rt_weather.humidity = jvh::getString(root, "SD");
                rt_weather.aqi = jvh::getString(root, "aqi");
                rt_weather.pm2p5 = jvh::getString(root, "aqi_pm25");
                rt_weather.update_time = jvh::getString(root, "time");

                // correct weather code to 'night' if time during 20:00 to 06:00
                if (const auto h = std::stoi(rt_weather.update_time.substr(0, 2));
                    h >= 20 || h < 6) {
                    rt_weather.weather_code[0] = L'n';
                }

                return true;
            } else {
                Logger::instance().error(tr::txt(tr::TID::ERR_INVALID_JSON));
                return false;
            }
        };

        if (!queryFrame(url_host, url_path, params, headers, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_RTW_FAILED));
        }

        return rt_weather;
    }

    auto queryRealtimeWeatherTownOrStreet(const std::string &code) {
        RealtimeWeather rt_weather{};

        constexpr std::string_view url_host = "https://forecast.weather.com.cn";
        const std::string url_path = std::format("/town/weather1dn/{}.shtml", code);

        utils::HttpParams params{};

        utils::HttpHeaders headers;
        headers.data.emplace("host", "forecast.weather.com.cn");

        auto func = [&rt_weather](const std::string &content) {
            static const std::regex pattern{ R"(forecast_default.*?(\{.+\})(?=;))" };

            if (const auto raw_data = regexExtractAllMatches(content, pattern); !raw_data.empty()) {
                const auto &data_obj = raw_data[0][1];

                const std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                    yyjson_read(data_obj.c_str(), data_obj.size(), 0),
                    [](yyjson_doc *p) { yyjson_doc_free(p); }
                );

                if (doc != nullptr) {
                    auto *root = yyjson_doc_get_root(doc.get());

                    auto toWString1dp = [](double value) {
                        // 四舍五入到 1 位小数，如果小数为0则返回整数部分
                        if (double rounded = std::round(value * 10.0) / 10.0;
                            std::abs(rounded - std::round(rounded)) < 1e-9) {
                            return std::format("{:.0f}", rounded);
                        } else {
                            return std::format("{:.1f}", rounded);
                        }
                    };

                    rt_weather.temp = toWString1dp(yyjson_get_real(yyjson_obj_get(root, "temp")));
                    rt_weather.update_time = jvh::getString(root, "time");
                    rt_weather.weather_text = jvh::getString(root, "weather");
                    rt_weather.weather_code = convWeatherTextToCode(rt_weather.weather_text);

                    // correct weather code to 'night' if time during 20:00 to 06:00
                    if (const auto h = std::stoi(rt_weather.update_time.substr(0, 2)); h >= 20 || h < 6) {
                        rt_weather.weather_code[0] = 'n';
                    }

                    {   // extract wind infomation
                        const auto &wind = jvh::getString(root, "wind");
                        if (const auto sep_idx = wind.find(' '); sep_idx != std::wstring::npos) {
                            rt_weather.wind_direction = wind.substr(0, sep_idx);
                            rt_weather.wind_strength = wind.substr(sep_idx + 1);
                        } else {
                            rt_weather.wind_direction = wind;
                        }

                        //rt_weather.wind_speed = "";
                    }

                    rt_weather.humidity = std::to_string(yyjson_get_int(yyjson_obj_get(root, "humidity")));
                    //rt_weather.aqi = "";
                    //rt_weather.pm2p5 = "";

                    return true;
                } else {
                    Logger::instance().error(tr::txt(tr::TID::ERR_INVALID_JSON));
                }
            } else {
                Logger::instance().error(tr::txt(tr::TID::ERR_EXTRACT_DATA_FAILED));
            }

            return false;
        };

        if (!queryFrame(url_host, url_path, params, headers, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_RTW_FAILED));
        }

        return rt_weather;
    }

    auto queryRealtimeWeather(const std::string &code) {
        if (code.size() == 9) {
            return queryRealtimeWeatherCity(code);
        }

        return queryRealtimeWeatherTownOrStreet(code);
    }

    auto queryForecastedWeather(const std::string &code) {
        std::array<ForecastedWeather, 3> fc_weather_3d{};

        std::string url_host, url_path;

        utils::HttpParams params{};
        utils::HttpHeaders headers{};

        if (code.size() == 9) {
            url_host = "https://www.weather.com.cn";
            url_path = std::format("/weathern/{}.shtml", code);

            headers.data.emplace("host", "www.weather.com.cn");
        } else {
            url_host = "https://forecast.weather.com.cn";
            url_path = std::format("/town/weathern/{}.shtml", code);

            headers.data.emplace("host", "forecast.weather.com.cn");
        }

        auto func = [&fc_weather_3d](const std::string &content) {
            static const std::regex pattern_temp_day{ "eventDay.*?(?=;)" };
            static const std::regex pattern_temp_night{ "eventNight.*?(?=;)" };
            static const std::regex pattern_temp_values{ R"((-?\d+))" };
            static const std::regex pattern_weather{ R"(<i class=\"item-icon ([dn]\d+) icons_bg" title=\"(.+?)\">)" };

            {
                auto temp_data_day = regexExtractAllMatches(content, pattern_temp_day);
                auto temp_data_night = regexExtractAllMatches(content, pattern_temp_night);
                if (!temp_data_day.empty() && !temp_data_night.empty()) {
                    auto temp_values_day = regexExtractAllMatches(temp_data_day[0][0], pattern_temp_values);
                    auto temp_values_night = regexExtractAllMatches(temp_data_night[0][0], pattern_temp_values);

                    if (temp_values_day.size() == temp_values_night.size() && temp_values_day.size() >= 4) {
                        for (size_t i = 0; i < 3; ++i) {
                            fc_weather_3d[i].temp_day = temp_values_day[i + 1][0];
                            fc_weather_3d[i].temp_night = temp_values_night[i + 1][0];
                        }
                    } else {
                        Logger::instance().error(std::format("{}(temperature)", tr::txt(tr::TID::ERR_EXTRACT_DATA_FAILED)));
                        return false;
                    }
                } else {
                    Logger::instance().error(tr::txt(tr::TID::ERR_EXTRACT_DATA_FAILED));
                    return false;
                }
            }

            {
                auto weather_data = regexExtractAllMatches(content, pattern_weather);
                if (weather_data.size() >= 6) {
                    for (size_t i = 0; i < 3; ++i) {
                        fc_weather_3d[i].weather_code_day = weather_data[i * 2 + 0][1];
                        fc_weather_3d[i].weather_text_day = weather_data[i * 2 + 0][2];

                        fc_weather_3d[i].weather_code_night = weather_data[i * 2 + 1][1];
                        fc_weather_3d[i].weather_text_night = weather_data[i * 2 + 1][2];
                    }
                } else {
                    Logger::instance().error(std::format("{}(weather)", tr::txt(tr::TID::ERR_EXTRACT_DATA_FAILED)));
                    return false;
                }
            }

            return true;
        };

        if (!queryFrame(url_host, url_path, params, headers, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_FCW3D_FAILED));
        }

        return fc_weather_3d;
    }

    auto queryWeatherAlerts(const std::string &code) {
        WeatherAlerts alerts{};

        constexpr std::string_view url_host = "https://d1.weather.com.cn";
        const std::string url_path = std::format("/dingzhi/{}.html", code);

        utils::HttpParams params;
        params.data.emplace("_", std::to_string(std::time(nullptr)));

        utils::HttpHeaders headers;
        headers.data.emplace("host", "d1.weather.com.cn");
        headers.data.emplace("referer", "https://www.weather.com.cn/");

        auto func = [&alerts](const std::string &response_content) {
            // locate data string
            const auto offset = response_content.find("alarmDZ");
            auto content = response_content.substr(response_content.find('{', offset));

            std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                yyjson_read(content.c_str(), content.size(), 0),
                [](yyjson_doc *p) { yyjson_doc_free(p); }
            );

            if (doc != nullptr) {
                auto *root = yyjson_doc_get_root(doc.get());

                auto extractAlertInfo = [](yyjson_val *j_val) {
                    return WeatherAlert{
                        .type = jvh::getString(j_val, "w5"),
                        .level = jvh::getString(j_val, "w7"),
                        .title = jvh::getString(j_val, "w13"),
                        .description = jvh::getString(j_val, "w9"),
                        .publish_time = jvh::getString(j_val, "w8"),
                    };
                };

                auto *j_arr_alerts = yyjson_obj_get(root, "w");
                size_t idx, max;
                yyjson_val *j_alert;
                yyjson_arr_foreach(j_arr_alerts, idx, max, j_alert) {
                    alerts.push_back(extractAlertInfo(j_alert));
                }

                return true;
            } else {
                Logger::instance().error(tr::txt(tr::TID::ERR_INVALID_JSON));
                return false;
            }
        };

        if (!queryFrame(std::string{ url_host }, url_path, params, headers, func)) {
            Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_RTWA_FAILED));
        }

        return alerts;
    }

    auto splitString(std::string_view str, char delimiter) {
        auto split_view = str | std::views::split(delimiter)
            | std::views::transform(
                [](auto&& range) { return std::string_view(range.begin(), range.end()); }
            );

        // 转换为 vector
        std::vector<std::string_view> tokens;
        for (auto token : split_view) {
            tokens.push_back(token);
        }

        return tokens;
    }

    bool queryLocations(const std::string &query, Locations &queried_locations) {
        queried_locations.clear();

        constexpr std::string_view url_host{ "https://toy1.weather.com.cn" };
        constexpr std::string_view url_path{ "/search" };

        utils::HttpParams params;
        params.data.emplace("cityname", query);
        params.data.emplace("callback", "success_jsonpCallback");
        params.data.emplace("_", std::to_string(std::time(nullptr)));

        utils::HttpHeaders headers;
        headers.data.emplace("host", "toy1.weather.com.cn");
        headers.data.emplace("referer", "https://www.weather.com.cn/");

        auto func = [&queried_locations](const std::string &content) {
            static const std::regex pattern_loc_record{ R"(\"ref\"\s*:\s*\"(.+?)\")" };
            const auto loc_records = regexExtractAllMatches(content, pattern_loc_record);

            if (!loc_records.empty()) {
                for (const auto &record : loc_records) {
                    if (const auto info_fields = splitString(record[1], '~');
                        info_fields.size() >= 10 && info_fields[0].size() >= 9) {
                        // skip scenic spot locations
                        if (std::isalpha(static_cast<unsigned char>(info_fields[0].back()))) {
                            continue;
                        }

                        Location loc;
                        loc.id = info_fields[0];
                        loc.name = info_fields[2];
                        if (info_fields[2] == info_fields[4]) {
                            loc.administrative_ownership = std::format("{}-{}", info_fields[2], info_fields[9]);
                        } else {
                            loc.administrative_ownership = std::format("{}-{}-{}", info_fields[2], info_fields[4], info_fields[9]);
                        }

                        queried_locations.push_back(std::move(loc));
                    }
                }
            }

            return true;
        };

        if (queryFrame(std::string{ url_host }, std::string{ url_path }, params, headers, func)) {
            return true;
        }

        Logger::instance().error(tr::txt(tr::TID::ERR_QUERY_GEOCODING_FAILED));
        return false;
    }

    std::string formatAlerts(const WeatherAlerts &alerts) {
        if (alerts.empty()) {
            return std::string{};
        }

        std::ostringstream oss;
        for (const auto &alt : alerts) {
            oss << alt.title << "\n";
            oss << "(" << alt.publish_time << ")\n";
            oss << alt.description << "\n\n";
        }

        return oss.str();
    }

    bool getLocation(Location &loc) {
        loc = {};
        constexpr std::string_view url_host{ "https://wgeo.weather.com.cn" };
        constexpr std::string_view url_path{ "/ip/" };

        utils::HttpParams params;

        const auto milliseconds_count = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        params.data.emplace("_", std::to_string(milliseconds_count));

        utils::HttpHeaders headers;
        headers.data.emplace("referer", "https://www.weather.com.cn/");
        headers.data.emplace("host", "wgeo.weather.com.cn");

        auto func = [&loc](const std::string &content) {
            static const std::regex pattern_id{ R"rgx(id\s*=\s*"(\d+)")rgx" };
            static const std::regex pattern_addr{ R"rgx(addr\s*=\s*"(.+)")rgx" };

            // parsing location id
            if (const auto matches_id = regexExtractAllMatches(content, pattern_id); !matches_id.empty()) {
                loc.id = matches_id[0][1];
            }

            // parsing location addr

            if (const auto matches_addr = regexExtractAllMatches(content, pattern_addr);
                !matches_addr.empty()) {
                if (const auto addr_parts = splitString(matches_addr[0][1], ',');
                    addr_parts.size() >= 3) {
                    loc.name = addr_parts[2];
                }
            }

            if (loc.id.empty() || loc.name.empty()) {
                Logger::instance().error(std::format("Parsing locaiton Fialed: {}", content));
                return false;
            }
            
            return true;
        };

        if (!queryFrame(url_host, url_path, params, headers, func)) {
            Logger::instance().error("Failed to query location from website");
        }

        return !loc.id.empty() && !loc.name.empty();
    }
}

std::string DataProviderSpiderWeatherComCn::WeatherDataBlock::getWeatherSummary() const {
    std::ostringstream oss;

    // weather & temperature
    oss << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::WEATHER_TEXT) << " "
        << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::TEMPERATURE) << " "
        << "(" << realtime_weather.update_time << ")";

    // wind
    oss << "\n" << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::WIND);

    // humidity
    oss << std::format(" {}: {}", tr::txt(tr::TID::FMT_HUMIDITY),
                       getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::HUMIDITY));

    // air quality
    oss << "\n"
        << std::format("{}: {}", tr::txt(tr::TID::FMT_AIR_QUALITY),
                       getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::AIR_QUALITY));
    oss << " PM2.5: " << getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::AIR_PM2P5);

    // weather alerts
    for (const auto &alert : weather_alerts) {
        oss << "\n" << std::format("[!] {} ({})", alert.title, alert.publish_time);
    }

    // forecasted weather
    auto fcw_formatter = [&oss, this](WeatherTimeSlot wt, std::string_view str_wt) {
        oss << "\n" << str_wt << ": " << getWeatherItem(wt, WeatherItem::WEATHER_TEXT)
            << " " << getWeatherItem(wt, WeatherItem::TEMPERATURE);
    };

    fcw_formatter(WeatherTimeSlot::TODAY, tr::txt(tr::TID::FMT_TODAY));
    fcw_formatter(WeatherTimeSlot::TOMMROW, tr::txt(tr::TID::FMT_TOMORROW));
    fcw_formatter(WeatherTimeSlot::DAY_AFTER_TOMMROW, tr::txt(tr::TID::FMT_DAT_AFTER_TOMORROW));

    oss << "\n";

    return oss.str();
}

std::string DataProviderSpiderWeatherComCn::WeatherDataBlock::getWeatherItem(WeatherTimeSlot time_slot, WeatherItem item) const {
    if (time_slot == WeatherTimeSlot::REALTIME) {
        if (item == WeatherItem::TEMPERATURE) {
            return std::format("{}℃", realtime_weather.temp);
        } else if (item == WeatherItem::WEATHER_TEXT) {
            return realtime_weather.weather_text;
        } else if (item == WeatherItem::WEATHER_CODE) {
            return realtime_weather.weather_code;
        } else if (item == WeatherItem::HUMIDITY) {
            return realtime_weather.humidity;
        } else if (item == WeatherItem::WIND) {
            return std::format("{} {}", realtime_weather.wind_direction, realtime_weather.wind_strength);
        } else if (item == WeatherItem::AIR_QUALITY) {
            return realtime_weather.aqi;
        } else if (item == WeatherItem::AIR_PM2P5) {
            return realtime_weather.pm2p5;
        } else if (item == WeatherItem::ALERTS) {
            return formatAlerts(weather_alerts);
        }
    } else {
        auto getForecastedWeatherContent = [](const ForecastedWeather &fcw, WeatherItem item) -> std::string {
            if (item == WeatherItem::TEMPERATURE) {
                return std::format("{}~{}℃", fcw.temp_night, fcw.temp_day);
            } else if (item == WeatherItem::WEATHER_TEXT) {
                return fcw.weather_text_day == fcw.weather_text_night ?
                    fcw.weather_text_day :
                    std::format("{}~{}", fcw.weather_text_day, fcw.weather_text_night);
            } else if (item == WeatherItem::WEATHER_CODE) {
                return fcw.weather_code_day;
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

bool DataProviderSpiderWeatherComCn::autoLocating(Location &loc) const {
    loc = {};

    if (!config.use_provider_location) {
        return false;
    }

    // 网站bug，单独获取一次位置不准确，需要连续获取两次
    getLocation(loc);
    return getLocation(loc);
}

bool DataProviderSpiderWeatherComCn::geocodingDirect(const std::string &query, Locations &queried_locations) const {
    if (queryLocations(query, queried_locations)) {
        if (queried_locations.empty()) {
            // if query is succeeded but reault is empty, split query by space and try again
            if (auto tokens = splitString(query, ' '); tokens.size() >= 2) {
                const std::string query_keyword{ tokens.back() }; tokens.pop_back();
                const std::string query_scope{ tokens.back() };

                if (Locations results;
                    queryLocations(query_keyword, results) && !results.empty()) {
                    std::ranges::copy_if(results, std::back_inserter(queried_locations),
                                         [&query_scope](const Location &loc) {
                        return loc.administrative_ownership.find(query_scope) != std::string::npos;
                    });
                }
            }
        }

        return true;
    }

    return false;
}

bool DataProviderSpiderWeatherComCn::geocodingReverse(const std::string &latitude, const std::string &longitude,
                                                      Locations &queried_locations) const {
    queried_locations.clear();
    return false;
}

WeatherDataCPtr DataProviderSpiderWeatherComCn::getWeatherData(const Location &loc) const {
    if (loc.id.empty()) {
        Logger::instance().error("No location id");
        return nullptr;
    }

    const auto data_block = std::make_shared<WeatherDataBlock>();
    data_block->realtime_weather = queryRealtimeWeather(loc.id);
    data_block->fc_weather_3d = queryForecastedWeather(loc.id);
    data_block->weather_alerts = queryWeatherAlerts(loc.id);

    return data_block;
}
