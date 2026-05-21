#include "pch.h"
#include "Common.h"
#include "resource.h"

#include <chrono>
#include <vector>
#include <unordered_map>

#include <WPCore/AppLocale.h>
#include <WPCore/utils.h>
#include <yyjson/src/yyjson.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Geolocation.h>

namespace cmn
{
    namespace
    {
        std::unordered_map<UINT, CString> string_res_map;
    }

    std::wstring MultiByte2WideChar(const std::string &str, bool from_utf8 /* = true */) {
        return MultiByte2WideChar(str.c_str(), from_utf8);
    }

    std::wstring MultiByte2WideChar(const char *str, bool from_utf8 /* = true */) {
        if (str == nullptr) {
            return {};
        }

        auto code_page = from_utf8 ? CP_UTF8 : CP_ACP;
        auto len = MultiByteToWideChar(code_page, 0, str, -1, nullptr, 0);
        if (len <= 0) {
            return {};
        }

        std::vector<wchar_t> buffer(len, 0);
        MultiByteToWideChar(code_page, 0, str, -1, buffer.data(), len);

        return { buffer.data() };
    }

    std::string WideChar2MultiByte(const std::wstring &str, bool to_utf8 /* = true */) {
        return WideChar2MultiByte(str.c_str(), to_utf8);
    }

    std::string WideChar2MultiByte(const wchar_t *str, bool to_utf8 /* = true */) {
        if (str == nullptr) {
            return {};
        }

        auto code_page = to_utf8 ? CP_UTF8 : CP_ACP;
        auto len = WideCharToMultiByte(code_page, 0, str, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            return {};
        }

        std::vector<char> buffer(len, 0);
        WideCharToMultiByte(code_page, 0, str, -1, buffer.data(), len, nullptr, nullptr);

        return { buffer.data() };
    }

    IniLoader::IniLoader(const CSimpleIniW &ini_file, std::wstring_view section) : ini_file_(&ini_file), section_(section) {
    }

    std::string IniLoader::GetValue(std::wstring_view key, std::wstring_view default_val/* = L""*/) const {
        return WideChar2MultiByte(GetValueW(key, default_val).c_str());
    }

    std::wstring IniLoader::GetValueW(std::wstring_view key, std::wstring_view default_val/* = L""*/) const {
        return ini_file_->GetValue(std::wstring{ section_ }.c_str(),
                                   std::wstring{ key }.c_str(),
                                   std::wstring{ default_val }.c_str());
    }

    bool IniLoader::GetBool(std::wstring_view key, bool default_val/* = false*/) const {
        return ini_file_->GetBoolValue(std::wstring{ section_ }.c_str(),
                                       std::wstring{ key }.c_str(), 
                                       default_val);
    }

    long IniLoader::GetLong(std::wstring_view key, long default_val/* = 0*/) const {
        return ini_file_->GetLongValue(std::wstring{ section_ }.c_str(),
                                       std::wstring{ key }.c_str(),
                                       default_val);
    }

    IniSaver::IniSaver(CSimpleIniW &ini_file, std::wstring_view section) : ini_file_(&ini_file), section_(section) {
    }

    void IniSaver::SetValue(std::wstring_view key, std::string_view val) const {
        SetValueW(key, MultiByte2WideChar(std::string{ val }.c_str()));
    }

    void IniSaver::SetValueW(std::wstring_view key, std::wstring_view val) const {
        ini_file_->SetValue(std::wstring{ section_ }.c_str(), 
                            std::wstring{ key }.c_str(), 
                            std::wstring{ val }.c_str());
    }

    void IniSaver::SetBool(std::wstring_view key, bool val) const {
        ini_file_->SetBoolValue(std::wstring{ section_ }.c_str(), 
                                std::wstring{ key }.c_str(), 
                                val);
    }

    void IniSaver::SetLong(std::wstring_view key, long val) const {
        ini_file_->SetLongValue(std::wstring{ section_ }.c_str(),
                                std::wstring{ key }.c_str(),
                                val);
    }

    const CString& GetStringRes(UINT str_id) {
        if (!string_res_map.contains(str_id)) {
            AFX_MANAGE_STATE(AfxGetStaticModuleState())
            
            CString str;
            if (!str.LoadStringW(str_id)) {
                str.Format(L"Unkown Text<%d>", str_id);
            }
            string_res_map.emplace(str_id, str);
        }

        return string_res_map.at(str_id);
    }

    CString GetString(WeatherTimeSlot time_slot) {
        switch (time_slot) {
            case WeatherTimeSlot::TODAY:
                return GetStringRes(IDS_TS_TODAY);

            case WeatherTimeSlot::TOMMROW:
                return GetStringRes(IDS_TS_TOMORROW);

            case WeatherTimeSlot::DAY_AFTER_TOMMROW:
                return GetStringRes(IDS_TS_DAY_AFT_TOM);

            case WeatherTimeSlot::REALTIME:
                return GetStringRes(IDS_TS_REALTIME);
        }

        return CString{};
    }

    CString GetString(WeatherItem item) {
        switch (item) {
            case WeatherItem::TEMPERATURE:
                return cmn::GetStringRes(IDS_WI_TEMPERATURE);

            case WeatherItem::WEATHER_TEXT:
                return cmn::GetStringRes(IDS_WI_WEATHER);

            case WeatherItem::HUMIDITY:
                return cmn::GetStringRes(IDS_WI_HUMIDITY);

            case WeatherItem::WIND:
                return cmn::GetStringRes(IDS_WI_WIND);

            case WeatherItem::AIR_QUALITY:
                return cmn::GetStringRes(IDS_WI_AIR_QUALITY);

            case WeatherItem::AIR_PM2P5:
                return cmn::GetStringRes(IDS_WI_AIR_PM2P5);

            case WeatherItem::AIR_PM10:
                return cmn::GetStringRes(IDS_WI_AIR_PM10);

            case WeatherItem::UV_INDEX:
                return cmn::GetStringRes(IDS_WI_UVI);

            case WeatherItem::PRECIPITATION:
                return cmn::GetStringRes(IDS_WI_PRECIPITATION);

            case WeatherItem::WEATHER_CODE:
            case WeatherItem::ALERTS:
                break;
        }

        return CString{};
    }
    
    bool GetSystemLocation(double &longitude, double &latitude) {
        using namespace winrt;
        using namespace Windows::Foundation;
        using namespace Windows::Devices::Geolocation;

        longitude = 0.0;
        latitude = 0.0;

        try
        {
            if (Geolocator::RequestAccessAsync().get() != GeolocationAccessStatus::Allowed)
            {
                return false;
            }

            Geolocator geolocator;
            geolocator.DesiredAccuracy(PositionAccuracy::Default);

            auto position = geolocator.GetGeopositionAsync(
                std::chrono::seconds{ 0 },
                std::chrono::seconds{ 5 }
            ).get();

            auto point = position.Coordinate().Point().Position();

            longitude = point.Longitude;
            latitude = point.Latitude;

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool GetIpLocation(double &longitude, double &latitude, std::string &name) {
        std::string page_name = tr::getLocale() == tr::Locale::CHINESE_S ? "cn" : "en";
        std::string url_path = std::format("/{}?json", page_name);

        std::string content;
        auto status_code = utils::internetGet("https://api.myip.la", url_path, content);
        if (status_code != 200 || content.empty()) {
            return false;
        }

        std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
            yyjson_read(content.c_str(), content.size(), 0),
            [](yyjson_doc *p) { yyjson_doc_free(p); }
        );

        if (doc == nullptr) {
            return false;
        }

        auto *root = yyjson_doc_get_root(doc.get());
        auto *obj_location = yyjson_obj_get(root, "location");
        if (obj_location == nullptr) {
            return false;
        }

        std::string str_longitude{ yyjson_get_str(yyjson_obj_get(obj_location, "longitude")) };
        std::string str_latitude{ yyjson_get_str(yyjson_obj_get(obj_location, "latitude")) };
        std::string province{ yyjson_get_str(yyjson_obj_get(obj_location, "province")) };
        std::string city{ yyjson_get_str(yyjson_obj_get(obj_location, "city")) };

        try {
            longitude = std::stod(str_longitude);
            latitude = std::stod(str_latitude);
            name = std::format("{} {}", province, city);

            return true;
        }
        catch (...) {
            return false;
        }
    }
}
