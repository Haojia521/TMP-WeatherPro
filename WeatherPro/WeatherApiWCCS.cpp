#include "pch.h"
#include "WeatherApiWCCS.h"

#include "Common.h"
#include "IconResources.h"

namespace
{
    constexpr std::wstring_view WSV_BLUE{ L"blue" };
    constexpr std::wstring_view WSV_ICON_STYLE{ L"icon_style" };
    constexpr std::wstring_view WSV_WCCS{ L"wccs" };
    constexpr std::wstring_view WSV_WHITE{ L"white" };
    constexpr std::wstring_view WSV_USE_PROVIDER_LOC{ L"use_provider_loc" };

    WapiWCCS::IconStyle ParseIconStyle(std::wstring_view str_icon_style) {
        if (str_icon_style == WSV_WHITE) {
            return WapiWCCS::IconStyle::White;
        } else {
            return WapiWCCS::IconStyle::Blue;    // default is blue
        }
    }

    const wchar_t* ToCharacters(WapiWCCS::IconStyle icon_style) {
        if (icon_style == WapiWCCS::IconStyle::White) {
            return WSV_WHITE.data();
        } else {
            return WSV_BLUE.data();
        }
    }
}

const IconSheet* WapiWCCS::GetWeatherIcons() const {
    return IconSheetManager::Instance().GetIconSheet(IconResType::WccBlue);
}

int WapiWCCS::GetWeatherIconIndex(std::wstring_view weather_code) const {
    // index  0 ~ 22 are white-style icons
    //       23 ~ 45 are blue-style icons
    //       46 ~ 55 are icons common to both styles
    static const std::unordered_map<std::wstring_view, int> code_index_map{
        {L"d01", 23},
        {L"n01", 24},
        {L"02", 25},
        {L"d03", 26},
        {L"n03", 27},
        {L"04", 28},
        {L"05", 29},
        {L"06", 30},
        {L"07", 31},
        {L"08", 32},
        {L"09", 33},
        {L"10", 34},
        {L"11", 35},
        {L"12", 36},
        {L"d13", 37},
        {L"n13", 38},
        {L"14", 39},
        {L"15", 40},
        {L"16", 41},
        {L"17", 42},
        {L"18", 43},
        {L"19", 44},
        {L"99", 45},
        {L"d00", 46},
        {L"n00", 47},
        {L"20", 48},
        {L"29", 49},
        {L"30", 50},
        {L"31", 51},
        {L"53", 52},
        {L"54", 53},
        {L"55", 54},
        {L"56", 55},
    };

    // return the icon of N/A by default
    int idx{ 45 };

    // find the icon according to weather code
    // replace the icon index if the icon is found
    if (code_index_map.contains(weather_code)) {
        idx = code_index_map.at(weather_code);
    }
    if (!weather_code.empty()) {
        if (const auto sub_code = weather_code.substr(1); code_index_map.contains(sub_code)) {
            idx = code_index_map.at(sub_code);
        }
    }

    // If the current setting is the white style and 
    // the index value is within the blue style range, convert the index to the white icon.
    if (config.icon_type == WapiWCCS::IconStyle::White && idx <= 45) {
        idx -= 23;
    }

    return idx;
}

DataProvider& WapiWCCS::GetProvider() {
    return provider_;
}

const DataProvider& WapiWCCS::GetProvider() const {
    return provider_;
}

DataProviderSpiderWeatherComCn& WapiWCCS::GetProviderWCCS() {
    return provider_;
}

const DataProviderSpiderWeatherComCn& WapiWCCS::GetProviderWCCS() const {
    return provider_;
}

void WapiWCCS::LoadConfig(const CSimpleIniW &ini_file) {
    cmn::IniLoader ini_helper(ini_file, WSV_WCCS);
    config.icon_type = ParseIconStyle(ini_helper.GetValueW(WSV_ICON_STYLE, WSV_BLUE));

    provider_.config.use_provider_location = ini_helper.GetBool(WSV_USE_PROVIDER_LOC, true);
}

void WapiWCCS::SaveConfig(CSimpleIniW &ini) const {
    cmn::IniSaver ini_helper(ini, WSV_WCCS);
    ini_helper.SetValueW(WSV_ICON_STYLE, ToCharacters(config.icon_type));

    ini_helper.SetBool(WSV_USE_PROVIDER_LOC, provider_.config.use_provider_location);
}
