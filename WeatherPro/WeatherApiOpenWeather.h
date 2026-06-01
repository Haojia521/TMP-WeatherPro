#pragma once

#include "WeatherAPI.h"

#include <WPCore/DataProviderOpenWeather.h>

class WapiOpenWeather final : public WeatherApi
{
public:
    const IconSheet* GetWeatherIcons() const override;
    int GetWeatherIconIndex(std::wstring_view weather_code) const override;
    DataProvider& GetProvider() override;
    const DataProvider& GetProvider() const override;
    void LoadConfig(const CSimpleIniW &ini_file) override;
    void SaveConfig(CSimpleIniW &ini) const override;

    DataProviderOpenWeather& GetProviderOpenWeather();
    const DataProviderOpenWeather& GetProviderOpenWeather() const;

private:
    DataProviderOpenWeather provider_;
};
