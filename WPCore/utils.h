#pragma once

#include <string>
#include <map>
#include <unordered_map>

namespace utils
{
    struct HttpParams
    {
        std::unordered_map<std::string, std::string> data;
    };

    struct HttpHeaders
    {
        std::unordered_map<std::string, std::string> data;
    };

    int internetGet(const std::string &host, const std::string &path, std::string &content,
                    const HttpParams &params = {}, const HttpHeaders &headers = {});

    int internetGetWithRetry(const std::string &host, const std::string &path, std::string &content,
                             const HttpParams &params = {}, const HttpHeaders &headers = {},
                             int retry_times = 3);

    std::tuple<std::string, std::string> generateEd25519Keypair();

    std::string timestamp_string(std::int64_t sec);
    std::string timestamp_string_date(std::int64_t sec);
    std::string timestamp_string_time(std::int64_t sec);
}
