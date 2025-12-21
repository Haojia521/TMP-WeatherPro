#pragma once

#include <httplib.h>

namespace utils
{
    std::wstring multiByte2WideChar(const char *str, bool from_utf8 = true);
    std::string wideChar2MultiByte(const wchar_t *str, bool to_utf8 = true);

    int internetGet(const std::string &host, const std::string &path, std::wstring &content,
                    httplib::Headers headers = httplib::Headers{});

    std::tuple<std::string, std::string> generateEd25519Keypair();
}
