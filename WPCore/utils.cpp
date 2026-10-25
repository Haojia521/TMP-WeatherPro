#include "utils.h"
#include "AppLocale.h"

#include <format>
#include <chrono>
#include <sstream>

#include <httplib.h>
#include <spdlog/spdlog.h>

namespace utils
{
    namespace
    {
        template <typename Map>
        std::string joinMapKeys(const Map& m) {
            std::string out = "[";
            bool first = true;
            for (const auto& [k, _] : m) {
                if (!first) out += ',';
                out += k;
                first = false;
            }
            out += ']';
            return out;
        }

        std::string_view lookupTextByCode(const std::unordered_map<std::string_view, tr::TID> &code_tid_map,
                                          const std::string_view code,
                                          const std::string_view func_name) {
            std::string lower_code{ code };
            std::ranges::transform(code, std::begin(lower_code),
                                   [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (code_tid_map.contains(lower_code)) {
                return tr::txt(code_tid_map.at(lower_code));
            }

            spdlog::error("Unkown code of {}: {}", func_name, code);
            return "???";
        }
    }

    int internetGet(const std::string &host, const std::string &path, std::string &content,
                    const HttpParams &params/* = {}*/, const HttpHeaders &headers/* = {}*/)
    {
        using namespace std::chrono_literals;

        content.clear();

        SPDLOG_DEBUG("internetGet: host={}, path={}, params={}, headers={}",
                     host, path, joinMapKeys(params.data), joinMapKeys(headers.data));

        httplib::Client clt(host);
        clt.set_connection_timeout(5s);
        clt.set_read_timeout(5s);

        httplib::Params lib_params;
        lib_params.insert(params.data.begin(), params.data.end());

        httplib::Headers lib_headers;
        lib_headers.insert(headers.data.begin(), headers.data.end());

        auto res = clt.Get(path, lib_params, lib_headers);

        if (res) {
            if (res->status >= 400) {
                spdlog::warn("internetGet: HTTP {}, host={}, path={}",
                             res->status, host, path);
            }

            content = std::move(res->body);
            return res->status;
        }

        spdlog::error("internetGet failed: {}. host={}, path={}, params={}, headers={}, "
                      "ssl_error={}, ssl_backend_error=0x{:X}",
                      httplib::to_string(res.error()),
                      host,
                      path,
                      joinMapKeys(params.data),
                      joinMapKeys(headers.data),
                      res.ssl_error(),
                      res.ssl_backend_error());
        return 0;
    }

    int internetGetWithRetry(const std::string &host, const std::string &path, std::string &content,
                             const HttpParams &params/* = {}*/, const HttpHeaders &headers/* = {}*/,
                             int max_attempts/* = 3*/) {
        // retry getting data if internet connection issues occurred

        max_attempts = std::clamp(max_attempts, 1, 10);    // avoid excessively large maximum retry count values

        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            auto status_code = internetGet(host, path, content, params, headers);

            // success
            if (status_code != 0) {
                if (attempt > 1) {
                    spdlog::info("internetGetWithRetry recovered: host={}, path={}, attempt={}/{}, status={}",
                                 host, path, attempt, max_attempts, status_code);
                }

                return status_code;
            }

            spdlog::warn("internetGetWithRetry: attempt {}/{} failed, host={}, path={}",
                         attempt, max_attempts, host, path);

            if (attempt == max_attempts) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(3));
        }

        spdlog::error("internetGetWithRetry exhausted: host={}, path={}, attempts={}",
                      host, path, max_attempts);
        return 0;
    }

    std::string timestamp_string(std::int64_t sec) {
        using namespace std::chrono;

        return std::format("{:%F %T}", sys_seconds{ seconds{sec} });
    }

    std::string timestamp_string_date(std::int64_t sec) {
        using namespace std::chrono;

        const auto dp = floor<days>(sys_seconds{ seconds{sec} });
        return std::format("{:%F}", year_month_day{ dp });
    }

    std::string timestamp_string_time(std::int64_t sec) {
        using namespace std::chrono;

        const auto tp = sys_seconds{ seconds{sec} };
        const auto day_time = tp - floor<days>(tp);
        return std::format("{:%T}", hh_mm_ss{ day_time });
    }

    std::int64_t parse_iso_datetime_to_utc_seconds(std::string_view iso_datatime) {
        using namespace std::chrono;

        std::string s(iso_datatime);

        auto parse_z = [](const std::string& text)
        {
            local_time<seconds> lt{};

            {
                std::istringstream iss(text);
                iss >> parse("%FT%T", lt);   // YYYY-MM-DDTHH:MM:SS
                if (!iss.fail()) {
                    return lt.time_since_epoch().count();
                }
            }

            {
                std::istringstream iss(text);
                iss >> parse("%FT%R", lt);   // YYYY-MM-DDTHH:MM
                if (!iss.fail()) {
                    return lt.time_since_epoch().count();
                }
            }

            throw std::runtime_error("failed to parse UTC datetime: " + text);
        };

        auto parse_with_offset = [](const std::string& text)
        {
            local_time<seconds> lt{};
            minutes offset{};

            {
                std::istringstream iss(text);
                iss >> parse("%FT%T%Ez", lt, offset); // YYYY-MM-DDTHH:MM:SS+08:00
                if (!iss.fail()) {
                    return (lt.time_since_epoch() - offset).count();
                }
            }

            {
                std::istringstream iss(text);
                iss >> parse("%FT%R%Ez", lt, offset); // YYYY-MM-DDTHH:MM+08:00
                if (!iss.fail()) {
                    return (lt.time_since_epoch() - offset).count();
                }
            }

            throw std::runtime_error("failed to parse offset datetime: " + text);
        };

        std::int64_t utc_seconds{};

        try {
            if (!s.empty() && s.back() == 'Z') {
                s.pop_back();
                utc_seconds = parse_z(s);
            } else {
                utc_seconds = parse_with_offset(s);
            }
        }
        catch (const std::exception &e) {
            spdlog::error(e.what());
        }

        return utc_seconds;
    }

    std::int64_t parse_iso_datetime_to_local_seconds(std::string_view iso_datatime) {
        using namespace std::chrono;

        const auto utc_seconds = parse_iso_datetime_to_utc_seconds(iso_datatime);
        return current_zone()->to_local(sys_seconds{ seconds{ utc_seconds } }).time_since_epoch().count();
    }

    std::string string_join(const std::vector<std::string> &parts, std::string_view delim) {
        std::string result;

        for (size_t i = 0; i < parts.size(); ++i) {
            if (i != 0) result.append(delim);
            result.append(parts[i]);
        }
        return result;
    }

    std::string_view get_direction_text(const std::string_view compass_code) {
        static const std::unordered_map<std::string_view, tr::TID> code_tid_map{
            {"n", tr::TID::FMT_DIRECTION_N,},
            {"nne", tr::TID::FMT_DIRECTION_NNE,},
            {"ne", tr::TID::FMT_DIRECTION_NE,},
            {"ene", tr::TID::FMT_DIRECTION_ENE,},
            {"e", tr::TID::FMT_DIRECTION_E,},
            {"ese", tr::TID::FMT_DIRECTION_ESE,},
            {"se", tr::TID::FMT_DIRECTION_SE,},
            {"sse", tr::TID::FMT_DIRECTION_SSE,},
            {"s", tr::TID::FMT_DIRECTION_S,},
            {"ssw", tr::TID::FMT_DIRECTION_SSW,},
            {"sw", tr::TID::FMT_DIRECTION_SW,},
            {"wsw", tr::TID::FMT_DIRECTION_WSW,},
            {"w", tr::TID::FMT_DIRECTION_W,},
            {"wnw", tr::TID::FMT_DIRECTION_WNW,},
            {"nw", tr::TID::FMT_DIRECTION_NW,},
            {"nnw", tr::TID::FMT_DIRECTION_NNW,},
            {"none", tr::TID::FMT_DIRECTION_NONE,},
            {"vrb", tr::TID::FMT_DIRECTION_VRB,},
        };

        return lookupTextByCode(code_tid_map, compass_code, "direction");
    }

    std::string_view get_precipitation_text(const std::string_view type_code) {
        static const std::unordered_map<std::string_view, tr::TID> code_tid_map{
            {"rain", tr::TID::FMT_PRECIPITATION_RAIN},
            {"snow", tr::TID::FMT_PRECIPITATION_SNOW},
            {"ice", tr::TID::FMT_PRECIPITATION_ICE},
            {"mixed", tr::TID::FMT_PRECIPITATION_MIXED},
            {"none", tr::TID::FMT_PRECIPITATION_NONE},
            {"unknown", tr::TID::FMT_PRECIPITATION_UNKNOWN},
        };

        return lookupTextByCode(code_tid_map, type_code, "precipitation");
    }
}
