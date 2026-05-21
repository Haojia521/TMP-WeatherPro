#pragma once

#include "ApiCollections.h"

#include <unordered_set>

enum class LocationSource : unsigned int
{
    ApiProvided = 1 << 0,
    OsGeolocation = 1 << 1,
    IpGeolocation = 1 << 2,
    IpRegionName = 1 << 3,

    None = 0,
    All = ApiProvided | OsGeolocation | IpGeolocation | IpRegionName,
};

inline constexpr LocationSource operator|(LocationSource lhs, LocationSource rhs) {
    return static_cast<LocationSource>(
        static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs)
        );
}

inline constexpr LocationSource operator&(LocationSource lhs, LocationSource rhs) {
    return static_cast<LocationSource>(
        static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs)
        );
}

inline constexpr LocationSource& operator|=(LocationSource& lhs, LocationSource rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline constexpr LocationSource& operator&=(LocationSource& lhs, LocationSource rhs) {
    lhs = lhs & rhs;
    return lhs;
}

inline constexpr bool HasFlag(LocationSource value, LocationSource flag) {
    return (static_cast<unsigned int>(value) & static_cast<unsigned int>(flag)) != 0;
}

class DataManager
{
    DataManager();
    ~DataManager() = default;
public:
    static DataManager& Instance();
    static bool IsUpdating();

    DataManager(const DataManager&) = delete;
    DataManager& operator=(const DataManager&) = delete;
    DataManager(DataManager&&) = delete;
    DataManager& operator=(DataManager&&) = delete;

    struct Snapshot
    {
        virtual ~Snapshot() = default;

        [[nodiscard]] virtual const wchar_t* GetTooltipText() const = 0;
        [[nodiscard]] virtual const wchar_t* GetWeatherItem(WeatherTimeSlot time_slot, WeatherItem item) const = 0;
        [[nodiscard]] virtual const WeatherData* GetWeatherData() const = 0;
    };

    [[nodiscard]]
    static std::shared_ptr<Snapshot> GetSnapshot();

    enum class UpdateInterval
    {
        Minutes5,
        Minutes15,
        Minutes30,
        Minutes60,
        Minutes120,
    };

    enum class LDoubleClickAction
    {
        OpenSettingWindow,
        UpdateWeather,
    };

    struct Config
    {
        ApiType api_type{ ApiType::WeatherComCnSpider };
        WeatherTimeSlot time_slot{ WeatherTimeSlot::REALTIME };
        UpdateInterval update_interval{ UpdateInterval::Minutes30 };
        LDoubleClickAction double_click_action{ LDoubleClickAction::OpenSettingWindow };
        LocationSource auto_locating_src{ LocationSource::All };
        bool show_weather_in_tooltip{ true };
        bool draw_weather_icon{ true };
        bool auto_locating{ false };
        bool draw_alerts_notification_dot{ true };

        std::unordered_set<WeatherDataKey, WeatherDataKeyHash> pinned_item_data_keys;
    };

    void SetCurrentLocation(const Location &loc);
    const Location& GetCurrentLocation() const;

    void LoadConfigs(std::wstring_view cfg_dir);
    void SaveConfigs() const;

    void UpdateWeather();
    void RefreshCache() const;

    WeatherApi& GetApi() const;
    ApiCollections& GetApiCollections() const;

    const Config& GetConfig() const;
    void SetConfig(const Config &cfg);

private:
    void ProceedUpdate();
    std::unique_ptr<ApiCollections> api_collections_;
    Location current_loc_;

    Config config_;
    std::wstring config_filepath_;
};


