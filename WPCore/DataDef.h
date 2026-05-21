#pragma once

#include <string>
#include <vector>
#include <memory>

enum class WeatherTimeSlot
{
    REALTIME,
    TODAY,
    TOMMROW,
    DAY_AFTER_TOMMROW,
};

enum class WeatherItem
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

struct WeatherData
{
    virtual ~WeatherData() = default;

    [[nodiscard]] virtual std::string getWeatherSummary() const = 0;
    [[nodiscard]] virtual std::string getWeatherItem(WeatherTimeSlot time_slot, WeatherItem item) const = 0;
};

using WeatherDataCPtr = std::shared_ptr<const WeatherData>;

struct Location
{
    std::string id;
    std::string name;
    std::string administrative_ownership;
    std::string longitude;
    std::string latitude;

    std::string getFormattedString() const;

    bool operator==(const Location&) const = default;
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

    [[nodiscard]] virtual WeatherDataCPtr getWeatherData(const Location &loc) const = 0;
};

using DataProviderPtr = std::shared_ptr<DataProvider>;
using DataProviderCPter = std::shared_ptr<const DataProvider>;
