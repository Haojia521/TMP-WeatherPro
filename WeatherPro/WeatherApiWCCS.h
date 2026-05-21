#pragma once

#include "WeatherAPI.h"

#include <WPCore/DataProviderSpiderWeatherComCn.h>

class WapiWCCS final : public WeatherApi
{
public:
    enum class IconStyle
    {
        Blue,
        White,
    };

    struct Config
    {
        IconStyle icon_type{ IconStyle::Blue };
    };

    const IconSheet* GetWeatherIcons() const override;
    int GetWeatherIconIndex(const std::string &weather_code) const override;
    DataProvider& GetProvider() override;
    const DataProvider& GetProvider() const override;
    void LoadConfig(const CSimpleIniW &ini_file) override;
    void SaveConfig(CSimpleIniW &ini) const override;

    DataProviderSpiderWeatherComCn& GetProviderWCCS();
    const DataProviderSpiderWeatherComCn& GetProviderWCCS() const;

    Config config;

private:
    DataProviderSpiderWeatherComCn provider_;
};
