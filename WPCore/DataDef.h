#pragma once

#include <string>
#include <vector>
#include <memory>

enum class WeatherTimeliness
{
    REALTIME,
    TODAY,
    TOMMROW,
    DAY_AFTER_TOMMROW,
};

enum class WeatherContent
{
    TEMPERATURE,
    WEATHER_TEXT,
    WEATHER_CODE,
    HUMIDITY,
    WIND,
    AIR_QUALITY,
    AIR_PM2P5,
    AIR_PM10,
    UV_INDEX,
    ALERTS,
    PRECIPITATION,
};

struct Location
{
    std::string id;
    std::string name;
    std::string administrative_ownership;
    std::string longitude;
    std::string latitude;
};

using Locations = std::vector<Location>;

class DataProvider
{
public:
    virtual ~DataProvider() = default;

    virtual bool autoLocating(Location &loc) const { return false; }
    virtual bool geocodingDirect(const std::string &query, Locations &queried_locations) const = 0;
    virtual bool geocodingReverse(const std::string &latitude, const std::string &longitude,
                                  Locations &queried_locations) const = 0;
    virtual bool fetchWeatherData(const Location &loc) = 0;

    virtual std::string getWeatherSummary() const = 0;
    virtual std::string getWeatherContent(WeatherTimeliness wt, WeatherContent wc) const = 0;
};

using DataProviderPtr = std::shared_ptr<DataProvider>;
