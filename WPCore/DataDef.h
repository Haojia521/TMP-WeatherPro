#pragma once

#include <string>
#include <vector>
#include <memory>
#include <bitset>

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
};

struct Location
{
    std::wstring id;
    std::wstring name;
    std::wstring administrative_ownership;
    std::wstring longitude;
    std::wstring latitude;
};

using Locaitons = std::vector<Location>;

class DataProvider
{
public:
    virtual ~DataProvider() = default;

    virtual bool queryLocations(const std::wstring &query, Locaitons &queriedLocations) = 0;
    virtual bool update() = 0;

    virtual std::wstring getWeatherSummary() = 0;
    virtual std::wstring getWeatherContent(WeatherTimeliness wt, WeatherContent wc) = 0;
};

using DataProviderPtr = std::shared_ptr<DataProvider>;
