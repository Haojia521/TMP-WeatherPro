#include "pch.h"
#include "WeatherApiOpenWeather.h"
#include "Common.h"

#include <WPCore/DataProviderOpenWeather.h>

namespace 
{
    constexpr std::wstring_view WSV_API_KEY{ L"api_key" };
    constexpr std::wstring_view WSV_IMPERIAL{ L"impreial" };
    constexpr std::wstring_view WSV_METRIC{ L"metric" };
    constexpr std::wstring_view WSV_OPEN_WEATHER{ L"open_weather" };
    constexpr std::wstring_view WSV_STANDARD{ L"standard" };
    constexpr std::wstring_view WSV_UNIT_TYPE{ L"unit_type" };
    constexpr std::wstring_view WSV_USE_ONECALL{ L"use_onecall" };

    DataProviderOpenWeather::UnitType ParseUnitType(std::wstring_view str_unit_type) {
        if (str_unit_type == WSV_STANDARD) {
            return DataProviderOpenWeather::UnitType::Standard;
        } else if (str_unit_type == WSV_IMPERIAL) {
            return DataProviderOpenWeather::UnitType::Imperial;
        } else {
            return DataProviderOpenWeather::UnitType::Metric;
        }
    }

    const wchar_t* ToCharactors(DataProviderOpenWeather::UnitType unit_type) {
        switch (unit_type) {
        case DataProviderOpenWeather::UnitType::Imperial:
            return WSV_IMPERIAL.data();

        case DataProviderOpenWeather::UnitType::Metric:
            return WSV_METRIC.data();

        case DataProviderOpenWeather::UnitType::Standard:
            return WSV_STANDARD.data();
        }

        return L"";
    }
}

const IconSheet* WapiOpenWeather::GetWeatherIcons() const {
    return IconSheetManager::Instance().GetIconSheet(IconResType::OpenWeather);
}

int WapiOpenWeather::GetWeatherIconIndex(const std::string &weather_code) const {
    static const std::unordered_map<std::string, int> code_index_map{
        {"01d", 0},
        {"02d", 2},
        {"03d", 4},
        {"04d", 5},
        {"09d", 6},
        {"10d", 7},
        {"11d", 9},
        {"13d", 10},
        {"50d", 11},
        {"01n", 1},
        {"02n", 3},
        {"03n", 4},
        {"04n", 5},
        {"09n", 6},
        {"10n", 8},
        {"11n", 9},
        {"13n", 10},
        {"50n", 11},
    };

    int idx{ 12 };

    if (code_index_map.contains(weather_code)) {
        idx = code_index_map.at(weather_code);
    }

    return idx;
}

DataProvider& WapiOpenWeather::GetProvider() {
    return provider_;
}

const DataProvider& WapiOpenWeather::GetProvider() const {
    return provider_;
}

DataProviderOpenWeather& WapiOpenWeather::GetProviderOpenWeather() {
    return provider_;
}

const DataProviderOpenWeather& WapiOpenWeather::GetProviderOpenWeather() const {
    return provider_;
}

void WapiOpenWeather::LoadConfig(const CSimpleIniW &ini_file) {
    const cmn::IniLoader ini_helper(ini_file, WSV_OPEN_WEATHER);
    provider_.config.api_key = ini_helper.GetValue(WSV_API_KEY);
    provider_.config.unit_type = ParseUnitType(ini_helper.GetValueW(WSV_UNIT_TYPE));
    provider_.config.use_onecall = ini_helper.GetBool(WSV_USE_ONECALL);
}

void WapiOpenWeather::SaveConfig(CSimpleIniW &ini_file) const {
    const cmn::IniSaver ini_helper(ini_file, WSV_OPEN_WEATHER);
    ini_helper.SetValue(WSV_API_KEY, provider_.config.api_key);
    ini_helper.SetValueW(WSV_UNIT_TYPE, ToCharactors(provider_.config.unit_type));
    ini_helper.SetBool(WSV_USE_ONECALL, provider_.config.use_onecall);
}
