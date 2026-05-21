#pragma once

#include "DataDef.h"

#include <array>
#include <functional>

class DataProviderQWeather : public DataProvider
{
public:
    struct ConfigApp
    {
        std::string app_key;
        std::string api_host;
        std::string project_id;
        std::string credential_id;
        std::string jwt_pub_key_file;
        std::string jwt_prv_key_file;
        bool enable_jwt{ false };

        bool operator==(const ConfigApp&) const = default;
    };

    struct ConfigFormatting
    {
        bool show_realtime_temp_feels_like{ false };
        bool show_realtime_wind{ true };
        bool show_realtime_wind_scale{ true };
        bool show_realtime_humidity{ true };
        bool show_realtime_pm2p5{ true };
        bool show_realtime_pm10{ false };
        bool show_forecasted_uv_index{ false };
        bool show_forecasted_humidity{ false };

        bool operator==(const ConfigFormatting&) const = default;
    };

    struct RealtimeWeather
    {
        std::string temp;
        std::string temp_feels_like;
        std::string weather_text;
        std::string weather_code;
        std::string wind_direction;
        std::string wind_scale;
        std::string wind_speed;
        std::string humidity;
        std::string update_time;
    };

    struct ForecastedWeather
    {
        std::string temp_max;
        std::string temp_min;
        std::string weather_day;
        std::string weather_night;
        std::string code_day;
        std::string code_night;
        std::string uv_index;
        std::string humidity;
    };

    struct AirQualityIndex
    {
        std::string code;
        std::string name;
        std::string aqi;
        std::string level;
        std::string category;
    };

    struct RealtimeAirQuality
    {
        std::vector<AirQualityIndex> indexes;
        std::string pm2p5;
        std::string pm10;
    };

    struct WeatherAlert
    {
        std::string sender_name;
        std::string issued_time;
        std::string severity;
        std::string color_code;
        std::string color;
        std::string expire_time;
        std::string headline;
        std::string description;
    };

    struct RealtimeWeatherAlerts
    {
        std::vector<WeatherAlert> alerts;
        std::vector<std::string> attributions;
    };

    struct WeatherDataBlock : WeatherData
    {
        explicit WeatherDataBlock(const ConfigFormatting &cfg_fmt);

        [[nodiscard]] std::string getWeatherSummary() const override;
        [[nodiscard]] std::string getWeatherItem(WeatherTimeSlot time_slot, WeatherItem item) const override;

        RealtimeWeather realtime_weather;
        RealtimeAirQuality realtime_air_quality;
        std::array<ForecastedWeather, 3> fc_weather_3d;
        RealtimeWeatherAlerts weather_alerts;

        std::reference_wrapper<const ConfigFormatting> config;
    };

    bool geocodingDirect(const std::string &query, Locations &queried_locations) const override;
    bool geocodingReverse(const std::string &latitude, const std::string &longitude, Locations &queried_locations) const override;

    [[nodiscard]] WeatherDataCPtr getWeatherData(const Location &loc) const override;

    ConfigApp config_app;
    ConfigFormatting config_fmt;
    
protected:
    [[nodiscard]] bool validateApiAuthentication() const;
};
