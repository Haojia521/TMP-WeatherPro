#pragma once

#include <simpleini/SimpleIni.h>
#include <WPCore/DataDef.h>

#include "IconResources.h"

class WeatherApi
{
public:
    virtual ~WeatherApi() = default;

    virtual const IconSheet* GetWeatherIcons() const = 0;
    virtual int GetWeatherIconIndex(const std::string &weather_code) const = 0;
    virtual DataProvider& GetProvider() = 0;
    virtual const DataProvider& GetProvider() const = 0;
    virtual void LoadConfig(const CSimpleIniW &ini_file) = 0;
    virtual void SaveConfig(CSimpleIniW &ini) const = 0;
};
