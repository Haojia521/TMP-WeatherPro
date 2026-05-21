#pragma once

#include "DataDef.h"

#include <array>

class DataProviderSpiderWeatherComCn : public DataProvider
{
public:
    struct RealtimeWeather
    {
        std::string temp;
        std::string weather_text;
        std::string weather_code;
        std::string wind_direction;
        std::string wind_strength;
        std::string wind_speed;
        std::string humidity;
        std::string aqi;
        std::string pm2p5;
        std::string update_time;
    };

    struct ForecastedWeather
    {
        std::string temp_day;
        std::string temp_night;
        std::string weather_text_day;
        std::string weather_text_night;
        std::string weather_code_day;
        std::string weather_code_night;
    };

    struct WeatherAlert
    {
        std::string type;
        std::string level;
        std::string title;
        std::string description;
        std::string publish_time;
    };

    using WeatherAlerts = std::vector<WeatherAlert>;

    struct Config
    {
        bool use_provider_location{ true };
    };

    struct WeatherDataBlock : WeatherData
    {
        [[nodiscard]] std::string getWeatherSummary() const override;
        [[nodiscard]] std::string getWeatherItem(WeatherTimeSlot time_slot, WeatherItem item) const override;

        RealtimeWeather realtime_weather;
        std::array<ForecastedWeather, 3> fc_weather_3d;
        WeatherAlerts weather_alerts;
    };

    bool autoLocating(Location &loc) const override;
    bool geocodingDirect(const std::string &query, Locations &queried_locations) const override;
    bool geocodingReverse(const std::string &latitude, const std::string &longitude, Locations &queried_locations) const override;

    [[nodiscard]] WeatherDataCPtr getWeatherData(const Location &loc) const override;

    Config config;
};
