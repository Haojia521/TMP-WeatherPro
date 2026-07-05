#include "pch.h"

#include "Common.h"
#include "IconResources.h"
#include "WeatherApiQWeather.h"

#include <algorithm>

namespace
{
    constexpr std::wstring_view WSV_API_HOST{ L"api_host" };
    constexpr std::wstring_view WSV_APP_KEY{ L"app_key" };
    constexpr std::wstring_view WSV_CRED_ID{ L"credential_id" };
    constexpr std::wstring_view WSV_ENABLE_JWT{ L"enable_jwt" };
    constexpr std::wstring_view WSV_FILL{ L"fill" };
    constexpr std::wstring_view WSV_HOLLOW{ L"hollow" };
    constexpr std::wstring_view WSV_ICON_STYLE{ L"icon_style" };
    constexpr std::wstring_view WSV_SHOW_FC_UVI{ L"show_forecasted_uv_index" };
    constexpr std::wstring_view WSV_SHOW_FC_HUM{ L"show_forecasted_humidity" };
    constexpr std::wstring_view WSV_SHOW_RT_HUM{ L"show_realtime_humidity" };
    constexpr std::wstring_view WSV_SHOW_RT_PM2P5{ L"show_realtime_pm2p5" };
    constexpr std::wstring_view WSV_SHOW_RT_PM10{ L"show_realtime_pm10" };
    constexpr std::wstring_view WSV_SHOW_RT_TEMP_FL{ L"show_realtime_temp_feels_like" };
    constexpr std::wstring_view WSV_SHOW_RT_WIND{ L"show_realtime_wind" };
    constexpr std::wstring_view WSV_SHOW_RT_WIND_SCALE{ L"show_realtime_wind_scale" };
    constexpr std::wstring_view WSV_PROJ_ID{ L"project_id" };
    constexpr std::wstring_view WSV_PRV_KEY_FILE{ L"private_key_filepath" };
    constexpr std::wstring_view WSV_PUB_KEY_FILE{ L"public_key_filepath" };
    constexpr std::wstring_view WSV_QWEATHER{ L"qweather" };

    WapiQWeather::IconStyle ParseIconStyle(std::wstring_view str_icon_style) {
        if (str_icon_style == WSV_HOLLOW) {
            return WapiQWeather::IconStyle::Hollow;
        } else {
            return WapiQWeather::IconStyle::Fill;    // default is Fill
        }
    }

    const wchar_t* ToCharacters(WapiQWeather::IconStyle icon_style) {
        if (icon_style == WapiQWeather::IconStyle::Hollow) {
            return WSV_HOLLOW.data();
        } else {
            return WSV_FILL.data();    // default is Fill
        }
    }
    
    constexpr std::array<std::wstring_view, 60> WEATHER_CODES{
        L"100", L"101", L"102", L"103", L"104", L"150", L"151", L"152", 
        L"153", L"300", L"301", L"302", L"303", L"304", L"305", L"306", 
        L"307", L"308", L"309", L"310", L"311", L"312", L"313", L"314", 
        L"315", L"316", L"317", L"318", L"350", L"351", L"399", L"400", 
        L"401", L"402", L"403", L"404", L"405", L"406", L"407", L"408", 
        L"409", L"410", L"456", L"457", L"499", L"500", L"501", L"502", 
        L"503", L"504", L"507", L"508", L"509", L"510", L"511", L"512", 
        L"513", L"514", L"515", L"999"
    };
}

const IconSheet* WapiQWeather::GetWeatherIcons() const {
    const auto icon_res_type = config.icon_style == IconStyle::Hollow ?
        IconResType::QWeatherHollow : IconResType::QWeatherFill;
    return IconManager::Instance().GetIconSheet(icon_res_type);
}

int WapiQWeather::GetWeatherIconIndex(std::wstring_view weather_code) const {
    if (auto it = std::ranges::find(WEATHER_CODES, weather_code);
        it != WEATHER_CODES.end()) {
        return static_cast<int>(std::distance(WEATHER_CODES.begin(), it));
    }

    return static_cast<int>(WEATHER_CODES.size() - 1);
}

DataProvider& WapiQWeather::GetProvider() {
    return provider_;
}

const DataProvider& WapiQWeather::GetProvider() const {
    return provider_;
}

DataProviderQWeather& WapiQWeather::GetProviderQWeather() {
    return provider_;
}

const DataProviderQWeather& WapiQWeather::GetProviderQWeather() const {
    return provider_;
}

void WapiQWeather::LoadConfig(const CSimpleIniW &ini_file) {
    cmn::IniLoader ini_helper(ini_file, WSV_QWEATHER);

    config.icon_style = ParseIconStyle(ini_helper.GetValueW(WSV_ICON_STYLE, WSV_FILL));

    provider_.config_app.app_key = ini_helper.GetValue(WSV_APP_KEY);
    provider_.config_app.api_host = ini_helper.GetValue(WSV_API_HOST);
    provider_.config_app.project_id = ini_helper.GetValue(WSV_PROJ_ID);
    provider_.config_app.credential_id = ini_helper.GetValue(WSV_CRED_ID);
    provider_.config_app.jwt_pub_key_file = ini_helper.GetValue(WSV_PUB_KEY_FILE);
    provider_.config_app.jwt_prv_key_file = ini_helper.GetValue(WSV_PRV_KEY_FILE);
    provider_.config_app.enable_jwt = ini_helper.GetBool(WSV_ENABLE_JWT, true);

    provider_.config_fmt.show_realtime_temp_feels_like = ini_helper.GetBool(WSV_SHOW_RT_TEMP_FL);
    provider_.config_fmt.show_realtime_wind = ini_helper.GetBool(WSV_SHOW_RT_WIND, true);
    provider_.config_fmt.show_realtime_wind_scale = ini_helper.GetBool(WSV_SHOW_RT_WIND_SCALE, true);
    provider_.config_fmt.show_realtime_humidity = ini_helper.GetBool(WSV_SHOW_RT_HUM, true);
    provider_.config_fmt.show_realtime_pm2p5 = ini_helper.GetBool(WSV_SHOW_RT_PM2P5, true);
    provider_.config_fmt.show_realtime_pm10 = ini_helper.GetBool(WSV_SHOW_RT_PM10);
    provider_.config_fmt.show_forecasted_uv_index = ini_helper.GetBool(WSV_SHOW_FC_UVI);
    provider_.config_fmt.show_forecasted_humidity = ini_helper.GetBool(WSV_SHOW_FC_HUM);
}

void WapiQWeather::SaveConfig(CSimpleIniW &ini) const {
    cmn::IniSaver ini_helper(ini, WSV_QWEATHER);

    ini_helper.SetValueW(WSV_ICON_STYLE, ToCharacters(config.icon_style));

    ini_helper.SetValue(WSV_APP_KEY, provider_.config_app.app_key);
    ini_helper.SetValue(WSV_API_HOST, provider_.config_app.api_host);
    ini_helper.SetValue(WSV_PROJ_ID, provider_.config_app.project_id);
    ini_helper.SetValue(WSV_CRED_ID, provider_.config_app.credential_id);
    ini_helper.SetValue(WSV_PUB_KEY_FILE, provider_.config_app.jwt_pub_key_file);
    ini_helper.SetValue(WSV_PRV_KEY_FILE, provider_.config_app.jwt_prv_key_file);
    ini_helper.SetBool(WSV_ENABLE_JWT, provider_.config_app.enable_jwt);

    ini_helper.SetBool(WSV_SHOW_RT_TEMP_FL, provider_.config_fmt.show_realtime_temp_feels_like);
    ini_helper.SetBool(WSV_SHOW_RT_WIND, provider_.config_fmt.show_realtime_wind);
    ini_helper.SetBool(WSV_SHOW_RT_WIND_SCALE, provider_.config_fmt.show_realtime_wind_scale);
    ini_helper.SetBool(WSV_SHOW_RT_HUM, provider_.config_fmt.show_realtime_humidity);
    ini_helper.SetBool(WSV_SHOW_RT_PM2P5, provider_.config_fmt.show_realtime_pm2p5);
    ini_helper.SetBool(WSV_SHOW_RT_PM10, provider_.config_fmt.show_realtime_pm10);
    ini_helper.SetBool(WSV_SHOW_FC_UVI, provider_.config_fmt.show_forecasted_uv_index);
    ini_helper.SetBool(WSV_SHOW_FC_HUM, provider_.config_fmt.show_forecasted_humidity);
}
