#include "utils.h"
#include "AppLocale.h"
#include "Logger.h"

#include <format>
#include <chrono>
#include <sstream>

#include <httplib.h>

namespace utils
{
    int internetGet(const std::string &host, const std::string &path, std::string &content,
                    const HttpParams &params/* = {}*/, const HttpHeaders &headers/* = {}*/)
    {
        using namespace std::chrono_literals;

        content.clear();

        httplib::Client clt(host);
        clt.set_connection_timeout(5s);
        clt.set_read_timeout(5s);

        httplib::Params lib_params;
        lib_params.insert(params.data.begin(), params.data.end());

        httplib::Headers lib_headers;
        lib_headers.insert(headers.data.begin(), headers.data.end());

        auto res = clt.Get(path, lib_params, lib_headers);

        if (res) {
            if (res->status == 404) return res->status;

            content = std::move(res->body);
            return res->status;
        } else {
            Logger::instance().error(
                std::format("INTERNET_RESPONSE_ERROR: {}, ssl_error={}, ssl_backend_error=0x{:X}",
                            httplib::to_string(res.error()),
                            res.ssl_error(),
                            res.ssl_backend_error())
            );
            return 0;
        }
    }

    int internetGetWithRetry(const std::string &host, const std::string &path, std::string &content,
                             const HttpParams &params/* = {}*/, const HttpHeaders &headers/* = {}*/,
                             int retry_times/* = 3*/) {
        // retry getting data if internet connection issues occurred
        auto status_code{ 0 };
        do {
            status_code = utils::internetGet(host, path, content, params, headers);
            if (status_code != 0) {
                break;
            }

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(5s);
            --retry_times;
        } while (retry_times > 0);

        return status_code;
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
            Logger::instance().error(e.what());
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
}
