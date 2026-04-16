#pragma once

#include "DataDef.h"

#include <array>

class DataProviderOpenWeather : public DataProvider
{
public:
    enum class UnitType
    {
        Standard,
        Metric,
        Imperial,
    };

    struct Config
    {
        std::string api_key;
        UnitType unit_type{ UnitType::Metric };
        bool use_onecall{ true };
    };

    struct RealtimeWeather
    {
        std::string weather_main;
        std::string weather_description;
        std::string weather_icon;
        std::string temp;
        std::string temp_feels_like;
        std::string humidity;
        std::string uv_index;
        std::string wind_speed;
        std::string wind_direction;
        std::string precipitation_rain_1h;
        std::string precipitation_snow_1h;
        std::string update_time;

        UnitType unit_type;
    };

    struct RealtimeAirQuality
    {
        std::string aqi;
        std::string pm2p5;
        std::string pm10;
    };

    struct ForecastedWeather
    {
        std::string weather_main;
        std::string weather_description;
        std::string weather_icon;
        std::string temp_max;
        std::string temp_min;
        std::string humidity;
        std::string uv_index;
        std::string wind_speed;
        std::string wind_direction;
        std::string precipitation_rain;
        std::string precipitation_snow;
        std::string precipitation_probability;

        UnitType unit_type;
    };

    struct WeatherAlert
    {
        std::string sender;
        std::string event;
        std::string start_datetime;
        std::string end_datetime;
        std::string description;
    };

    using WeatherAlerts = std::vector<WeatherAlert>;

    bool geocodingDirect(const std::string &query, Locations &queried_locations) const override;
    bool geocodingReverse(const std::string &latitude, const std::string &longitude, Locations &queried_locations) const override;

    bool fetchWeatherData(const Location &loc) override;

    std::string getWeatherSummary() const override;
    std::string getWeatherContent(WeatherTimeliness wt, WeatherContent wc) const override;

    bool validateApiAuthentication() const;

    Config config_;

    RealtimeWeather realtime_weather_;
    RealtimeAirQuality realtime_air_;
    std::array<ForecastedWeather, 3> fc_weather_3d_;
    WeatherAlerts alerts_;
};
