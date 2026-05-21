#pragma once

#include "WeatherAPI.h"

#include <WPCore/DataProviderQWeather.h>

class WapiQWeather final : public WeatherApi
{
public:
    enum class IconStyle
    {
        Fill,
        Hollow,
    };

    struct Config
    {
        IconStyle icon_style{ IconStyle::Fill };
    };

    const IconSheet* GetWeatherIcons() const override;
    int GetWeatherIconIndex(const std::string &weather_code) const override;
    DataProvider& GetProvider() override;
    const DataProvider& GetProvider() const override;
    void LoadConfig(const CSimpleIniW &ini_file) override;
    void SaveConfig(CSimpleIniW &ini) const override;

    DataProviderQWeather& GetProviderQWeather();
    const DataProviderQWeather& GetProviderQWeather() const;

    Config config;

private:
    DataProviderQWeather provider_;
};
