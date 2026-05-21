#pragma once

#include <WPCore/DataDef.h>

enum class ApiType
{
    WeatherComCnSpider,
    QWeather,
    OpenWeather,
};

enum class IconResType
{
    WccBlue,
    WccWhite,
    QWeatherFill,
    QWeatherHollow,
    OpenWeather,
    Loading,
};

struct WeatherDataKey
{
    WeatherTimeSlot time_slot;
    WeatherItem item;

    bool operator==(const WeatherDataKey&) const = default;
};

struct WeatherDataKeyHash
{
    uint64_t operator()(const WeatherDataKey &wdk) const noexcept {
        return (static_cast<uint64_t>(wdk.time_slot) << 32) | static_cast<uint64_t>(wdk.item);
    }
};
