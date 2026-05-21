#pragma once

#include <WPCore/DataDef.h>
#include <simpleini/SimpleIni.h>
#include <string>

namespace cmn
{
    std::wstring MultiByte2WideChar(const std::string &str, bool from_utf8 = true);
    std::wstring MultiByte2WideChar(const char *str, bool from_utf8 = true);

    std::string WideChar2MultiByte(const std::wstring &str, bool to_utf8 = true);
    std::string WideChar2MultiByte(const wchar_t *str, bool to_utf8 = true);

    class IniLoader
    {
        const CSimpleIniW *ini_file_{ nullptr };
        std::wstring_view section_;
    public:
        IniLoader(const CSimpleIniW &ini_file, std::wstring_view section);

        IniLoader(const IniLoader&) = delete;
        IniLoader& operator=(const IniLoader&) = delete;

        std::wstring GetValueW(std::wstring_view key, std::wstring_view default_val = L"") const;
        std::string GetValue(std::wstring_view key, std::wstring_view default_val = L"") const;
        bool GetBool(std::wstring_view key, bool default_val = false) const;
        long GetLong(std::wstring_view key, long default_val = 0) const;
    };

    class IniSaver
    {
        CSimpleIniW *ini_file_;
        std::wstring_view section_;
    public:
        IniSaver(CSimpleIniW &ini_file, std::wstring_view section);

        void SetValueW(std::wstring_view key, std::wstring_view val) const;
        void SetValue(std::wstring_view key, std::string_view val) const;
        void SetBool(std::wstring_view key, bool val) const;
        void SetLong(std::wstring_view key, long val) const;
    };

    const CString& GetStringRes(UINT str_id);
    CString GetString(WeatherTimeSlot time_slot);
    CString GetString(WeatherItem item);

    bool GetSystemLocation(double &longitude, double &latitude);
    bool GetIpLocation(double &longitude, double &latitude, std::string &name);
}
