#pragma once

#include <string>
#include <unordered_map>

namespace utils
{
    std::wstring multiByte2WideChar(const char *str, bool from_utf8 = true);
    std::string wideChar2MultiByte(const wchar_t *str, bool to_utf8 = true);

    int internetGet(const std::string &host, const std::string &path, std::string &content,
                    std::unordered_map<std::string, std::string> headers = {});

    std::tuple<std::string, std::string> generateEd25519Keypair();

    std::string timestamp_string(std::int64_t sec);
    std::string timestamp_string_date(std::int64_t sec);
    std::string timestamp_string_time(std::int64_t sec);
}
