#include "DataProviderSpiderWeatherComCn.h"
#include "AppLocale.h"
#include "Logger.h"
#include "utils.h"

#include <regex>
#include <ranges>
#include <sstream>

#include <yyjson.h>

#include <fstream>

namespace swcc
{
    // weather text-code mapping
    namespace tcm
    {
        struct TcPair
        {
            std::string_view text;
            std::string_view code;
        };

        constexpr std::array<TcPair, 43> mapping_table = { {
            {"晴", "d00"},
            {"多云", "d01"},
            {"阴", "d02"},
            {"阵雨", "d03"},
            {"雷阵雨", "d04"},
            {"雷阵雨伴有冰雹", "d05"},
            {"雨夹雪", "d06"},
            {"小雨", "d07"},
            {"中雨", "d08"},
            {"大雨", "d09"},
            {"暴雨", "d10"},
            {"大暴雨", "d11"},
            {"特大暴雨", "d12"},
            {"阵雪", "d13"},
            {"小雪", "d14"},
            {"中雪", "d15"},
            {"大雪", "d16"},
            {"暴雪", "d17"},
            {"雾", "d18"},
            {"冻雨", "d19"},
            {"沙尘暴", "d20"},
            {"小到中雨", "d21"},
            {"中到大雨", "d22"},
            {"大到暴雨", "d23"},
            {"暴雨到大暴雨", "d24"},
            {"大暴雨到特大暴雨", "d25"},
            {"小到中雪", "d26"},
            {"中到大雪", "d27"},
            {"大到暴雪", "d28"},
            {"浮尘", "d29"},
            {"扬沙", "d30"},
            {"强沙尘暴", "d31"},
            {"霾", "d53"},
            {"无", "d99"},
            {"浓雾", "d32"},
            {"强浓雾", "d49"},
            {"中度霾", "d54"},
            {"重度霾", "d55"},
            {"严重霾", "d56"},
            {"大雾", "d57"},
            {"特强浓雾", "d58"},
            {"雨", "d97"},
            {"雪", "d98"},
        } };

        static constexpr std::string_view text_to_code(std::string_view text) {
            auto itr = std::find_if(
                std::begin(mapping_table), std::end(mapping_table),
                [text](const TcPair &p) { return p.text == text; }
            );

            return itr != mapping_table.end() ? itr->code : "d99";
        }

        static constexpr std::string_view code_to_text(std::string_view code) {
            auto itr = std::find_if(
                std::begin(mapping_table), std::end(mapping_table),
                [code](const TcPair &p) { return p.code == code; }
            );

            return itr != mapping_table.end() ? itr->text : "无";
        }
    }

    using RegexMatch = std::vector<std::string>;
    using RegexMatches = std::vector<RegexMatch>;

    static RegexMatches regexExtractAllMatches(const std::string &text, const std::regex &pattern) {
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

    inline static std::string jsonGetStr(yyjson_val *j_val, const char *key) {
        return yyjson_get_str(yyjson_obj_get(j_val, key));
    }

    static const std::string agent{ "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36 Edg/143.0.0.0" };

    using RealtimeWeather = DataProviderSpiderWeatherComCn::RealtimeWeather;
    using ForecastedWeather = DataProviderSpiderWeatherComCn::ForecastedWeather;
    using WeatherAlert = DataProviderSpiderWeatherComCn::WeatherAlert;
    using WeatherAlerts = DataProviderSpiderWeatherComCn::WeatherAlerts;

    static bool queryRealtimeWeatherCity(const std::string &code, RealtimeWeather &rt_weather) {
        rt_weather = RealtimeWeather{};

        std::string url_host = "https://d1.weather.com.cn";
        std::string url_path = std::format("/sk_2d/{}.html?_={}", code, std::time(nullptr));

        std::unordered_map<std::string, std::string> headers{
            {"host", "d1.weather.com.cn"},
            {"referer", "https://www.weather.com.cn/"},
            {"user-agent", agent},
        };

        std::string content;
        auto status_code = utils::internetGet(url_host, url_path, content, headers);

        // remove "var dataSK = " from content
        content = content.substr(content.find("{"));

        bool succeed{ false };
        if (!content.empty()) {
            std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                yyjson_read(content.c_str(), content.size(), 0),
                [](yyjson_doc *p) { yyjson_doc_free(p); }
            );

            if (doc != nullptr) {
                auto *root = yyjson_doc_get_root(doc.get());

                rt_weather.temp = jsonGetStr(root, "temp");
                rt_weather.weather_text = jsonGetStr(root, "weather");
                rt_weather.weather_code = jsonGetStr(root, "weathercode");
                rt_weather.wind_direction = jsonGetStr(root, "WD");
                rt_weather.wind_strength = jsonGetStr(root, "WS");
                rt_weather.wind_speed = jsonGetStr(root, "wse");
                rt_weather.humidity = jsonGetStr(root, "SD");
                rt_weather.aqi = jsonGetStr(root, "aqi");
                rt_weather.pm2p5 = jsonGetStr(root, "aqi_pm25");
                rt_weather.update_time = jsonGetStr(root, "time");

                // correct weather code to 'night' if time during 20:00 to 06:00
                auto h = std::stoi(rt_weather.update_time.substr(0, 2));
                if (h >= 20 || h < 6) {
                    rt_weather.weather_code[0] = L'n';
                }

                succeed = true;
            } else {
                Logger::instance().error(tr::txt(tr::TextID::ERR_INVALID_JSON));
            }
        } else {
            Logger::instance().error(std::format("[{}] {}", status_code, tr::txt(tr::TextID::ERR_INTERNET_EMPTY_RESPONSE)));
        }

        if (!succeed) {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_RTW_FAILED));
        }

        return succeed;
    }

    static bool queryRealtimeWeatherTownOrStreet(const std::string &code, RealtimeWeather &rt_weather) {
        rt_weather = RealtimeWeather{};

        std::string url_host = "https://forecast.weather.com.cn";
        std::string url_path = std::format("/town/weather1dn/{}.shtml", code);

        std::unordered_map<std::string, std::string> headers = {
            {"host", "forecast.weather.com.cn"},
            {"user-agent", agent},
        };

        std::string content;
        auto status_code = utils::internetGet(url_host, url_path, content, headers);

        bool succeed{ false };
        if (!content.empty()) {
            static const std::regex pattern{ R"(forecast_default.*?(\{.+\})(?=;))" };

            auto raw_data = regexExtractAllMatches(content, pattern);
            if (!raw_data.empty()) {
                const auto &data_obj = raw_data[0][1];

                std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                    yyjson_read(data_obj.c_str(), data_obj.size(), 0),
                    [](yyjson_doc *p) { yyjson_doc_free(p); }
                );

                if (doc != nullptr) {
                    auto *root = yyjson_doc_get_root(doc.get());

                    auto toWString1dp = [](double value) {
                        // 四舍五入到 1 位小数
                        double rounded = std::round(value * 10.0) / 10.0;

                        // 判断是否为整数（允许浮点误差）
                        if (std::abs(rounded - std::round(rounded)) < 1e-9) {
                            return std::format("{:.0f}", rounded);
                        } else {
                            return std::format("{:.1f}", rounded);
                        }
                    };

                    rt_weather.temp = toWString1dp(yyjson_get_real(yyjson_obj_get(root, "temp")));
                    rt_weather.update_time = jsonGetStr(root, "time");
                    rt_weather.weather_text = jsonGetStr(root, "weather");
                    rt_weather.weather_code = tcm::text_to_code(rt_weather.weather_text);

                    // correct weather code to 'night' if time during 20:00 to 06:00
                    auto h = std::stoi(rt_weather.update_time.substr(0, 2));
                    if (h >= 20 || h < 6) {
                        rt_weather.weather_code[0] = 'n';
                    }

                    {   // extract wind infomation
                        const auto &wind = jsonGetStr(root, "wind");
                        auto sep_idx = wind.find(' ');
                        if (sep_idx != std::wstring::npos) {
                            rt_weather.wind_direction = wind.substr(0, sep_idx);
                            rt_weather.wind_strength = wind.substr(sep_idx + 1);
                        } else {
                            rt_weather.wind_direction = wind;
                        }

                        rt_weather.wind_speed = "";
                    }

                    rt_weather.humidity = std::to_string(yyjson_get_int(yyjson_obj_get(root, "humidity")));
                    rt_weather.aqi = "";
                    rt_weather.pm2p5 = "";

                    succeed = true;
                } else {
                    Logger::instance().error(tr::txt(tr::TextID::ERR_INVALID_JSON));
                }
            } else {
                Logger::instance().error(tr::txt(tr::TextID::ERR_EXTRACT_DATA_FAILED));
            }
        } else {
            Logger::instance().error(std::format("[{}] {}", status_code, tr::txt(tr::TextID::ERR_INTERNET_EMPTY_RESPONSE)));
        }

        if (!succeed) {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_RTW_FAILED));
        }

        return succeed;
    }

    static bool queryRealtimeWeather(const std::string &code, RealtimeWeather &rt_weather) {
        if (code.size() == 9) {
            return queryRealtimeWeatherCity(code, rt_weather);
        } else {
            return queryRealtimeWeatherTownOrStreet(code, rt_weather);
        }
    }

    static bool queryForecastedWeather(const std::string &code, std::array<ForecastedWeather, 3> &fc_weather_3d) {
        fc_weather_3d = std::array<ForecastedWeather, 3>{};

        std::string url_host, url_path;
        std::unordered_map<std::string, std::string> headers{
            {"user-agent", agent},
        };

        if (code.size() == 9) {
            url_host = "https://www.weather.com.cn";
            url_path = std::format("/weathern/{}.shtml", code);

            headers.insert({ "host", "www.weather.com.cn" });
        } else {
            url_host = "https://forecast.weather.com.cn";
            url_path = std::format("/town/weathern/{}.shtml", code);

            headers.insert({ "host", "forecast.weather.com.cn" });
        }

        std::string content;
        auto status_code = utils::internetGet(url_host, url_path, content, headers);

        auto continue_parsing{ status_code == 200 };
        if (!content.empty()) {
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
                        Logger::instance().error(std::format("{}(temperature)", tr::txt(tr::TextID::ERR_EXTRACT_DATA_FAILED)));
                        continue_parsing = false;
                    }
                } else {
                    Logger::instance().error(tr::txt(tr::TextID::ERR_EXTRACT_DATA_FAILED));
                    continue_parsing = false;
                }
            }

            if (continue_parsing) {
                auto weather_data = regexExtractAllMatches(content, pattern_weather);
                if (weather_data.size() >= 6) {
                    for (size_t i = 0; i < 3; ++i) {
                        fc_weather_3d[i].weather_code_day = weather_data[i * 2 + 0][1];
                        fc_weather_3d[i].weather_text_day = weather_data[i * 2 + 0][2];

                        fc_weather_3d[i].weather_code_night = weather_data[i * 2 + 1][1];
                        fc_weather_3d[i].weather_text_night = weather_data[i * 2 + 1][2];
                    }
                } else {
                    Logger::instance().error(std::format("{}(weather)", tr::txt(tr::TextID::ERR_EXTRACT_DATA_FAILED)));
                    continue_parsing = false;
                }
            }
        } else {
            Logger::instance().error(std::format("[{}] {}", status_code, tr::txt(tr::TextID::ERR_INTERNET_EMPTY_RESPONSE)));
            continue_parsing = false;
        }

        if (!continue_parsing) {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_FCW3D_FAILED));
        }

        return continue_parsing;
    }

    static bool queryWeatherAlerts(const std::string &code, WeatherAlerts &alerts) {
        alerts.clear();

        std::string url_host = "https://d1.weather.com.cn";
        std::string url_path = std::format("/dingzhi/{}.html?_={}", code, std::time(nullptr));

        std::unordered_map<std::string, std::string> headers = {
            {"host", "d1.weather.com.cn"},
            {"referer", "https://www.weather.com.cn/"},
            {"user-agent", agent},
        };

        std::string content;
        auto status_code = utils::internetGet(url_host, url_path, content, headers);

        // locate data string
        auto offset = content.find("alarmDZ");
        content = content.substr(content.find("{", offset));

        bool succeed{ false };
        if (!content.empty()) {
            std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                yyjson_read(content.c_str(), content.size(), 0),
                [](yyjson_doc *p) { yyjson_doc_free(p); }
            );

            if (doc != nullptr) {
                auto *root = yyjson_doc_get_root(doc.get());

                auto extractAlertInfo = [](yyjson_val *j_val) {
                    return WeatherAlert{
                        .type = jsonGetStr(j_val, "w5"),
                        .level = jsonGetStr(j_val, "w7"),
                        .title = jsonGetStr(j_val, "w13"),
                        .description = jsonGetStr(j_val, "w9"),
                        .publish_time = jsonGetStr(j_val, "w8"),
                    };
                };

                auto *j_arr_alerts = yyjson_obj_get(root, "w");
                size_t idx{ 0 }, max{ 0 };
                yyjson_val *j_alert{ nullptr };
                yyjson_arr_foreach(j_arr_alerts, idx, max, j_alert) {
                    alerts.push_back(extractAlertInfo(j_alert));
                }

                succeed = true;
            } else {
                Logger::instance().error(tr::txt(tr::TextID::ERR_INVALID_JSON));
            }
        } else {
            Logger::instance().error(std::format("[{}] {}", status_code, tr::txt(tr::TextID::ERR_INTERNET_EMPTY_RESPONSE)));
        }

        if (!succeed) {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_RTWA_FAILED));
        }

        return succeed;
    }

    static auto splitString(std::string_view str, char delimiter) {
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

    static bool queryLocations(const std::string &query, Locations &queried_locations) {
        queried_locations.clear();

        std::string url_host = "https://toy1.weather.com.cn";
        std::string url_path = std::format("/search?cityname={}&callback=success_jsonpCallback&_={}",
                                           query, std::time(nullptr));

        std::unordered_map<std::string, std::string> headers{
            {"host", "toy1.weather.com.cn"},
            {"referer", "https://www.weather.com.cn/"},
            {"user-agent", agent},
        };

        std::string content;
        auto status_code = utils::internetGet(url_host, url_path, content, headers);

        auto succeed{ false };
        if (!content.empty()) {
            static const std::regex pattern_loc_record{ R"(\"ref\"\s*:\s*\"(.+?)\")" };
            auto loc_records = regexExtractAllMatches(content, pattern_loc_record);

            if (!loc_records.empty()) {
                for (const auto &record : loc_records) {
                    auto info_fields = splitString(record[1], '~');

                    if (info_fields.size() >= 10 && info_fields[0].size() >= 9) {
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

            succeed = true;
        } else {
            Logger::instance().error(std::format("[{}] {}", status_code, tr::txt(tr::TextID::ERR_INTERNET_EMPTY_RESPONSE)));
        }

        if (!succeed) {
            Logger::instance().error(tr::txt(tr::TextID::ERR_QUERY_GEOCODING_FAILED));
        }

        return succeed;
    }

    static std::string formatAlerts(const WeatherAlerts &alerts) {
        if (alerts.empty()) {
            return std::string{};
        }

        std::ostringstream oss;
        for (const auto &alt : alerts) {
            oss << alt.title << std::endl;
            oss << "(" << alt.publish_time << ")" << std::endl;
            oss << alt.description << std::endl << std::endl;
        }

        return oss.str();
    }
}

bool DataProviderSpiderWeatherComCn::autoLocating(Location &loc) const {
    loc = Location{};

    std::string url_host{ "https://wgeo.weather.com.cn" };

    auto milliseconds_count = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    std::string url_path = std::format("/ip/?_={}", milliseconds_count);

    std::unordered_map<std::string, std::string> headers = {
        {"referer", "https://www.weather.com.cn/"},
        {"host", "wgeo.weather.com.cn"},
        {"user-agent", swcc::agent},
    };

    std::string content;
    auto status_code = utils::internetGet(url_host, url_path, content, headers);

    bool succeed{ false };
    if (!content.empty()) {
        static const std::regex pattern_id{ R"rgx(id\s*=\s*"(\d+)")rgx" };
        static const std::regex pattern_addr{ R"rgx(addr\s*=\s*"(.+)")rgx" };
        
        // parsing location id
        auto matches_id = swcc::regexExtractAllMatches(content, pattern_id);
        if (!matches_id.empty()) {
            loc.id = matches_id[0][1];
        }

        // parsing location addr
        auto matches_addr = swcc::regexExtractAllMatches(content, pattern_addr);
        if (!matches_addr.empty()) {
            auto addr_parts = swcc::splitString(matches_addr[0][1], ',');
            if (addr_parts.size() >= 3) {
                loc.name = addr_parts[2];
            }
        }

        if (loc.id.empty() || loc.name.empty()) {
            Logger::instance().error(std::format("Parsing locaiton Fialed: {}", content));
        } else {
            succeed = true;
        }
    } else {
        Logger::instance().error(std::format("[{}] {}", status_code, tr::txt(tr::TextID::ERR_INTERNET_EMPTY_RESPONSE)));
    }

    if (!succeed) {
        Logger::instance().error("Failed to query location from website");
    }

    return !loc.id.empty() && !loc.name.empty();
}

bool DataProviderSpiderWeatherComCn::geocodingDirect(const std::string &query, Locations &queried_locations) const {
    return swcc::queryLocations(query, queried_locations);
}

bool DataProviderSpiderWeatherComCn::geocodingReverse(const std::string &latitude, const std::string &longitude,
                                                      Locations &queried_locations) const {
    queried_locations.clear();
    return false;
}

bool DataProviderSpiderWeatherComCn::fetchWeatherData(const Location &loc) {
    if (loc.id.empty()) {
        Logger::instance().error("No location id");
        return false;
    }

    auto all_succeeded{ true };

    all_succeeded &= swcc::queryRealtimeWeather(loc.id, realtime_weather_);
    all_succeeded &= swcc::queryForecastedWeather(loc.id, fc_weather_3d_);
    all_succeeded &= swcc::queryWeatherAlerts(loc.id, weather_alerts_);

    return all_succeeded;
}

std::string DataProviderSpiderWeatherComCn::getWeatherContent(WeatherTimeliness wt, WeatherContent wc) const {
    if (wt == WeatherTimeliness::REALTIME) {
        if (wc == WeatherContent::TEMPERATURE) {
            return realtime_weather_.temp;
        } else if (wc == WeatherContent::WEATHER_TEXT) {
            return realtime_weather_.weather_text;
        } else if (wc == WeatherContent::WEATHER_CODE) {
            return realtime_weather_.weather_code;
        } else if (wc == WeatherContent::HUMIDITY) {
            return realtime_weather_.humidity;
        } else if (wc == WeatherContent::WIND) {
            return std::format("{} {}", realtime_weather_.wind_direction, realtime_weather_.wind_strength);
        } else if (wc == WeatherContent::AIR_QUALITY) {
            return realtime_weather_.aqi;
        } else if (wc == WeatherContent::AIR_PM2P5) {
            return realtime_weather_.pm2p5;
        } else if (wc == WeatherContent::ALERTS) {
            return swcc::formatAlerts(weather_alerts_);
        }
    } else {
        auto getForecastedWeatherContent = [](const ForecastedWeather &fcw, WeatherContent wc) -> std::string {
            if (wc == WeatherContent::TEMPERATURE) {
                return std::format("{}~{}℃", fcw.temp_night, fcw.temp_day);
            } else if (wc == WeatherContent::WEATHER_TEXT) {
                return fcw.weather_text_day == fcw.weather_text_night ?
                    fcw.weather_text_day :
                    std::format("{}~{}", fcw.weather_text_day, fcw.weather_text_night);
            } else if (wc == WeatherContent::WEATHER_CODE) {
                return fcw.weather_code_day;
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

std::string DataProviderSpiderWeatherComCn::getWeatherSummary() const {
    std::ostringstream oss;

    // weather & temperature
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
    oss << " PM2.5: " << getWeatherContent(WeatherTimeliness::REALTIME, WeatherContent::AIR_PM2P5);

    oss << std::endl;

    // weather alerts
    for (const auto &alert : weather_alerts_) {
        oss << std::format("[!] {} ({})", alert.title, alert.publish_time) << std::endl;
    }

    // forecasted weather
    auto fcw_formatter = [&oss, this](WeatherTimeliness wt, const std::string &str_wt) {
        oss << str_wt << ": " << getWeatherContent(wt, WeatherContent::WEATHER_TEXT)
            << " " << getWeatherContent(wt, WeatherContent::TEMPERATURE);
        oss << std::endl;
    };

    fcw_formatter(WeatherTimeliness::TODAY, tr::txt(tr::TextID::FMT_TODAY));
    fcw_formatter(WeatherTimeliness::TOMMROW, tr::txt(tr::TextID::FMT_TOMORROW));
    fcw_formatter(WeatherTimeliness::DAY_AFTER_TOMMROW, tr::txt(tr::TextID::FMT_DAT_AFTER_TOMORROW));

    return oss.str();
}
