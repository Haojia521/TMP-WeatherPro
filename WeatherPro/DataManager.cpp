#include "pch.h"

#include "DataManager.h"
#include "Common.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
#include <regex>

namespace fs = std::filesystem;

namespace
{
    constexpr std::wstring_view WSV_CONFIG_FILENAME{ L"WeatherPro.ini" };
    constexpr std::wstring_view WSV_WEATHER_PRO{ L"weatherpro" };
    constexpr std::wstring_view WSV_TIME_SLOT{ L"time_slot" };
    constexpr std::wstring_view WSV_REALTIME{ L"realtime" };
    constexpr std::wstring_view WSV_TODAY{ L"today" };
    constexpr std::wstring_view WSV_TOMORROW{ L"tomorrow" };
    constexpr std::wstring_view WSV_DAY_AFTER_TOMORROW{ L"day_after_tomorrow" };
    constexpr std::wstring_view WSV_UPDATE_INTERVAL{ L"update_interval" };
    constexpr std::wstring_view WSV_MINUTES_5{ L"5min" };
    constexpr std::wstring_view WSV_MINUTES_15{ L"15min" };
    constexpr std::wstring_view WSV_MINUTES_30{ L"30min" };
    constexpr std::wstring_view WSV_MINUTES_60{ L"60min" };
    constexpr std::wstring_view WSV_MINUTES_120{ L"120min" };
    constexpr std::wstring_view WSV_L_DOUBLE_CLICK_ACTION{ L"l_double_click_action" };
    constexpr std::wstring_view WSV_UPDATE_WEATHER{ L"update_weather" };
    constexpr std::wstring_view WSV_OPEN_SETTINGS{ L"open_settings" };
    constexpr std::wstring_view WSV_SHOW_WEATHER_IN_TOOLTIP{ L"show_weather_in_tooltip" };
    constexpr std::wstring_view WSV_DRAW_WEATHER_ICON{ L"draw_weather_icon" };
    constexpr std::wstring_view WSV_ACTIVE_AUTO_LOCATING{ L"auto_locating" };
    constexpr std::wstring_view WSV_API_TYPE{ L"api_type" };
    constexpr std::wstring_view WSV_API_WCC{ L"api_weather.com.cn" };
    constexpr std::wstring_view WSV_API_QWEATHER{ L"api_qweather" };
    constexpr std::wstring_view WSV_API_OPENWEATHER{ L"api_openweather" };
    constexpr std::wstring_view WSV_LOCATION_ID{ L"location_id" };
    constexpr std::wstring_view WSV_LOCATION_NAME{ L"location_name" };
    constexpr std::wstring_view WSV_LOC_LONGITUDE{ L"loc_longitude" };
    constexpr std::wstring_view WSV_LOC_LATITUDE{ L"loc_latitude" };
    constexpr std::wstring_view WSV_AUTO_LOCATING_SOURCE{ L"auto_locating_source" };
    constexpr std::wstring_view WSV_DRAW_ALERTS_NOTIFICATION_DOT{ L"draw_alerts_notification_dot" };
    constexpr std::wstring_view WSV_PINNED_ITEM_DATA_KEYS{ L"pinned_item_data_keys" };
    constexpr std::wstring_view WSV_MAIN_ITEM_SCROLL_TEXT{ L"main_item_scroll_text" };
    constexpr std::wstring_view WSV_FORMAT_GEO_COORDS_IN_SUMMARY{ L"format_geo_coords_in_summary" };

    template<typename ENM_T, typename STR_T>
    struct EnumStrMappingItem
    {
        ENM_T enum_val;
        STR_T str_val;
    };

    template<typename ENM_T, typename STR_T, size_t N>
    ENM_T ParseStrToEnum(const std::array<EnumStrMappingItem<ENM_T, STR_T>, N> &mapping_list,
                         const STR_T &str, ENM_T default_val) {
        const auto itr = std::ranges::find_if(
            mapping_list,
            [&str](const auto &item)
        {
            return item.str_val == str;
        });

        if (itr != mapping_list.end()) {
            return itr->enum_val;
        }

        return default_val;
    }

    template<typename ENM_T, typename STR_T, size_t N>
    STR_T::const_pointer ConvertEnumToCharactors(const std::array<EnumStrMappingItem<ENM_T, STR_T>, N> &mapping_list,
                                                 ENM_T enm, const STR_T &default_val) {
        const auto itr = std::ranges::find_if(
            mapping_list,
            [enm](const auto &item)
        {
            return item.enum_val == enm;
        });

        if (itr != mapping_list.end()) {
            return itr->str_val.data();
        }

        return default_val.data();
    }

    constexpr auto enum_str_mapping_update_interval = std::to_array({
        EnumStrMappingItem{.enum_val = DataManager::UpdateInterval::Minutes5, .str_val = WSV_MINUTES_5},
        EnumStrMappingItem{.enum_val = DataManager::UpdateInterval::Minutes15, .str_val = WSV_MINUTES_15},
        EnumStrMappingItem{.enum_val = DataManager::UpdateInterval::Minutes30, .str_val = WSV_MINUTES_30},
        EnumStrMappingItem{.enum_val = DataManager::UpdateInterval::Minutes60, .str_val = WSV_MINUTES_60},
        EnumStrMappingItem{.enum_val = DataManager::UpdateInterval::Minutes120, .str_val = WSV_MINUTES_120},
    });

    constexpr auto enum_str_mapping_l_double_click_action = std::to_array({
        EnumStrMappingItem{.enum_val = DataManager::LDoubleClickAction::OpenSettingWindow, .str_val = WSV_OPEN_SETTINGS},
        EnumStrMappingItem{.enum_val = DataManager::LDoubleClickAction::UpdateWeather, .str_val = WSV_UPDATE_WEATHER},
    });

    constexpr auto enum_str_mapping_time_slot = std::to_array({
        EnumStrMappingItem{.enum_val = WeatherTimeSlot::REALTIME, .str_val = WSV_REALTIME},
        EnumStrMappingItem{.enum_val = WeatherTimeSlot::TODAY, .str_val = WSV_TODAY},
        EnumStrMappingItem{.enum_val = WeatherTimeSlot::TOMMROW, .str_val = WSV_TOMORROW},
        EnumStrMappingItem{.enum_val = WeatherTimeSlot::DAY_AFTER_TOMMROW, .str_val = WSV_DAY_AFTER_TOMORROW},
    });

    constexpr auto enum_str_mapping_api_type = std::to_array({
        EnumStrMappingItem{.enum_val = ApiType::WeatherComCnSpider, .str_val = WSV_API_WCC},
        EnumStrMappingItem{.enum_val = ApiType::QWeather, .str_val = WSV_API_QWEATHER},
        EnumStrMappingItem{.enum_val = ApiType::OpenWeather, .str_val = WSV_API_OPENWEATHER},
    });

    DataManager::UpdateInterval ParseUpdateInterval(std::wstring_view str_interval) {
        return ParseStrToEnum(enum_str_mapping_update_interval, 
                              str_interval, DataManager::UpdateInterval::Minutes30);
    }

    const wchar_t* ToCharactors(DataManager::UpdateInterval interval) {
        return ConvertEnumToCharactors(enum_str_mapping_update_interval, 
                                       interval, WSV_MINUTES_30);
    }

    DataManager::LDoubleClickAction ParseLDoubleClickAction(std::wstring_view str_ldc_action) {
        return ParseStrToEnum(enum_str_mapping_l_double_click_action,
                              str_ldc_action, DataManager::LDoubleClickAction::OpenSettingWindow);
    }

    const wchar_t* ToCharactors(DataManager::LDoubleClickAction ldc_action) {
        return ConvertEnumToCharactors(enum_str_mapping_l_double_click_action,
                                       ldc_action, WSV_OPEN_SETTINGS);
    }

    WeatherTimeSlot ParseTimeSlot(std::wstring_view time_slot) {
        return ParseStrToEnum(enum_str_mapping_time_slot,
                              time_slot, WeatherTimeSlot::REALTIME);
    }

    const wchar_t* ToCharactors(WeatherTimeSlot time_slot) {
        return ConvertEnumToCharactors(enum_str_mapping_time_slot,
                                       time_slot, WSV_REALTIME);
    }

    ApiType ParseApiType(std::wstring_view str_api_type) {
        return ParseStrToEnum(enum_str_mapping_api_type,
                              str_api_type, ApiType::WeatherComCnSpider);
    }

    const wchar_t* ToCharactors(ApiType api_type) {
        return ConvertEnumToCharactors(enum_str_mapping_api_type,
                                       api_type, WSV_API_WCC);
    }

    std::wstring FormatLocation(const Location &loc, bool format_geo_coords) {
        return cmn::MultiByte2WideChar(loc.getFormattedString(format_geo_coords).c_str());
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// 自动定位
    bool AutoLocating(LocationSource source, const DataProvider &data_provider, Location &loc) {
        loc = {};

        // 1. 通过 provider 获取位置
        if (HasFlag(source, LocationSource::ApiProvided) && data_provider.autoLocating(loc)) {
            return true;
        }

        auto assign_first_location = [&loc](const Locations &locations) {
            if (locations.empty()) {
                return false;
            }

            loc = locations.front();
            return true;
        };

        auto try_reverse_geocoding = [&](double latitude, double longitude) {
            Locations locations;
            return data_provider.geocodingReverse(
                std::format("{:.2f}", latitude),
                std::format("{:.2f}", longitude),
                locations) &&
                assign_first_location(locations);
        };

        auto try_direct_geocoding = [&](const std::string &loc_name) {
            if (loc_name.empty()) {
                return false;
            }

            Locations locations;
            return data_provider.geocodingDirect(loc_name, locations) &&
                assign_first_location(locations);
        };

        double longitude = 0.0;
        double latitude = 0.0;
        std::string loc_name;

        // 2. 通过系统获取经纬度
        if (HasFlag(source, LocationSource::OsGeolocation) && cmn::GetSystemLocation(longitude, latitude)) {
            if (try_reverse_geocoding(latitude, longitude)) {
                return true;
            }
        }

        // 3. 通过 IP 获取经纬度和地名
        if ((HasFlag(source, LocationSource::IpGeolocation) || HasFlag(source, LocationSource::IpRegionName)) &&
            cmn::GetIpLocation(longitude, latitude, loc_name)) {
            if (HasFlag(source, LocationSource::IpGeolocation) && 
                try_reverse_geocoding(latitude, longitude)) {
                return true;
            }

            if (HasFlag(source, LocationSource::IpRegionName) && 
                try_direct_geocoding(loc_name)) {
                return true;
            }
        }

        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// 数据更新与缓存
    
    std::mutex update_mutex;
    std::atomic_bool is_updating{ false };

    struct UpdateGuard {
        UpdateGuard() {
            is_updating.store(true, std::memory_order_release);
        }
        ~UpdateGuard() {
            is_updating.store(false, std::memory_order_release);
        }
    };

    std::atomic<std::shared_ptr<const WeatherData>> current_weather_data;

    using WeatherCacheMap = std::unordered_map<WeatherDataKey, std::wstring, WeatherDataKeyHash>;

    struct SnapshotImpl : DataManager::Snapshot
    {
        [[nodiscard]]
        const wchar_t* GetTooltipText() const override {
            return tooltip_text.c_str();
        }

        [[nodiscard]]
        const wchar_t* GetWeatherItem(WeatherTimeSlot time_slot, WeatherItem item) const override {
            if (const auto id_pair = WeatherDataKey{ .time_slot = time_slot, .item = item };
                cache_map.contains(id_pair)) {
                return cache_map.at(id_pair).c_str();
            }

            return L"--";
        }

        [[nodiscard]]
        const WeatherData* GetWeatherData() const override {
            return base_data.get();
        }

        WeatherCacheMap cache_map;
        std::wstring tooltip_text;

        std::shared_ptr<const WeatherData> base_data;
    };

    std::atomic<std::shared_ptr<SnapshotImpl>> cache_snapshot;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// 解析与保存常驻区设置
    auto ParsePinnedItemDataKeys(const std::wstring &str_keys) {
        static const std::wregex pattern{ LR"rgx(\(\s*(\d+)\s*,\s*(\d+)\s*\))rgx" };

        auto begin = std::wsregex_iterator(str_keys.begin(), str_keys.end(), pattern);
        auto end = std::wsregex_iterator();

        std::unordered_set<WeatherDataKey, WeatherDataKeyHash> keys;
        for (auto it{ begin }; it != end; ++it) {
            keys.emplace(static_cast<WeatherTimeSlot>(std::stoi((*it)[1].str())),
                         static_cast<WeatherItem>(std::stoi((*it)[2].str())));
        }

        return keys;
    }

    const std::wstring ToWString(const std::unordered_set<WeatherDataKey, WeatherDataKeyHash> &keys) {
        std::wstring str_keys;
        str_keys.reserve(keys.size() * 8);

        for (const auto &k : keys) {
            str_keys += std::format(L"({},{})", static_cast<int>(k.time_slot), static_cast<int>(k.item));
        }

        return str_keys;
    }
}

DataManager::DataManager() {
    api_collections_ = std::make_unique<ApiCollections>();
}

DataManager& DataManager::Instance() {
    static DataManager manager;
    return manager;
}

void DataManager::SetCurrentLocation(const Location &loc) {
    current_loc_ = loc;
}

const Location& DataManager::GetCurrentLocation() const {
    return current_loc_;
}

void DataManager::LoadConfigs(std::wstring_view cfg_dir) {
    const fs::path config_directory{ cfg_dir };
    if (!fs::is_directory(config_directory)) {
        return;
    }

    const auto config_filepath = config_directory / WSV_CONFIG_FILENAME;
    config_filepath_ = config_filepath;

    CSimpleIniW ini_file;
    ini_file.SetUnicode();
    ini_file.LoadFile(config_filepath.c_str());

    const cmn::IniLoader ini_helper(ini_file, WSV_WEATHER_PRO);
    // location
    current_loc_.id = ini_helper.GetValue(WSV_LOCATION_ID);
    current_loc_.name = ini_helper.GetValue(WSV_LOCATION_NAME);
    current_loc_.longitude = ini_helper.GetValue(WSV_LOC_LONGITUDE);
    current_loc_.latitude = ini_helper.GetValue(WSV_LOC_LATITUDE);

    // settings
    config_.api_type = ParseApiType(ini_helper.GetValueW(WSV_API_TYPE));
    config_.time_slot = ParseTimeSlot(ini_helper.GetValueW(WSV_TIME_SLOT));
    config_.update_interval = ParseUpdateInterval(ini_helper.GetValueW(WSV_UPDATE_INTERVAL));
    config_.double_click_action = ParseLDoubleClickAction(ini_helper.GetValueW(WSV_L_DOUBLE_CLICK_ACTION));
    config_.auto_locating_src = static_cast<LocationSource>(
        ini_helper.GetLong(WSV_AUTO_LOCATING_SOURCE, static_cast<long>(LocationSource::All)));
    config_.show_weather_in_tooltip = ini_helper.GetBool(WSV_SHOW_WEATHER_IN_TOOLTIP, true);
    config_.draw_weather_icon = ini_helper.GetBool(WSV_DRAW_WEATHER_ICON, true);
    config_.auto_locating = ini_helper.GetBool(WSV_ACTIVE_AUTO_LOCATING);
    config_.draw_alerts_notification_dot = ini_helper.GetBool(WSV_DRAW_ALERTS_NOTIFICATION_DOT, true);
    config_.main_item_scroll_text = ini_helper.GetBool(WSV_MAIN_ITEM_SCROLL_TEXT, true);
    config_.format_geo_coords_in_summary = ini_helper.GetBool(WSV_FORMAT_GEO_COORDS_IN_SUMMARY, true);

    config_.pinned_item_data_keys = ParsePinnedItemDataKeys(ini_helper.GetValueW(WSV_PINNED_ITEM_DATA_KEYS));

    api_collections_->LoadConfigs(ini_file);
}

void DataManager::SaveConfigs() const {
    CSimpleIniW ini_file;
    ini_file.SetUnicode();

    const cmn::IniSaver ini_helper(ini_file, WSV_WEATHER_PRO);
    // location
    ini_helper.SetValue(WSV_LOCATION_ID, current_loc_.id);
    ini_helper.SetValue(WSV_LOCATION_NAME, current_loc_.name);
    ini_helper.SetValue(WSV_LOC_LONGITUDE, current_loc_.longitude);
    ini_helper.SetValue(WSV_LOC_LATITUDE, current_loc_.latitude);

    // settings
    ini_helper.SetValueW(WSV_API_TYPE, ToCharactors(config_.api_type));
    ini_helper.SetValueW(WSV_TIME_SLOT, ToCharactors(config_.time_slot));
    ini_helper.SetValueW(WSV_UPDATE_INTERVAL, ToCharactors(config_.update_interval));
    ini_helper.SetValueW(WSV_L_DOUBLE_CLICK_ACTION, ToCharactors(config_.double_click_action));
    ini_helper.SetLong(WSV_AUTO_LOCATING_SOURCE, static_cast<long>(config_.auto_locating_src));
    ini_helper.SetBool(WSV_SHOW_WEATHER_IN_TOOLTIP, config_.show_weather_in_tooltip);
    ini_helper.SetBool(WSV_DRAW_WEATHER_ICON, config_.draw_weather_icon);
    ini_helper.SetBool(WSV_ACTIVE_AUTO_LOCATING, config_.auto_locating);
    ini_helper.SetBool(WSV_DRAW_ALERTS_NOTIFICATION_DOT, config_.draw_alerts_notification_dot);
    ini_helper.SetBool(WSV_MAIN_ITEM_SCROLL_TEXT, config_.main_item_scroll_text);
    ini_helper.SetBool(WSV_FORMAT_GEO_COORDS_IN_SUMMARY, config_.format_geo_coords_in_summary);

    ini_helper.SetValueW(WSV_PINNED_ITEM_DATA_KEYS, ToWString(config_.pinned_item_data_keys));

    api_collections_->SaveConfigs(ini_file);

    ini_file.SaveFile(config_filepath_.c_str());    // todo: make logging if any error occurs
}

WeatherApi& DataManager::GetApi() const {
    return api_collections_->GetApi(config_.api_type);
}

ApiCollections& DataManager::GetApiCollections() const {
    return *api_collections_;
}

void DataManager::UpdateWeather() {
    std::thread update_thread(&DataManager::ProceedUpdate, this);
    update_thread.detach();
}

bool DataManager::IsUpdating() {
    return is_updating.load(std::memory_order_acquire);
}

void DataManager::ProceedUpdate() {
    std::unique_lock lock{ update_mutex, std::try_to_lock };

    if (!lock.owns_lock()) {
        return;
    }

    // ensure that the 'is_updating' flag is set to false
    // after exiting the update process.
    UpdateGuard guard;    

    const auto &current_api = api_collections_->GetApi(config_.api_type);
    const auto &data_provider = current_api.GetProvider();
    
    if (Location loc; 
        config_.auto_locating && AutoLocating(config_.auto_locating_src, data_provider, loc)) {
        current_loc_ = loc;

        // save the location to config file
        SaveConfigs();
    }

    auto weather_data = current_api.GetProvider().getWeatherData(current_loc_);
    current_weather_data.store(weather_data, std::memory_order_release);

    RefreshCache();
}

void DataManager::RefreshCache() const {
    auto new_snapshot = std::make_shared<SnapshotImpl>();

    auto weather_data = current_weather_data.load(std::memory_order_acquire);
    if (weather_data != nullptr) {
        new_snapshot->base_data = weather_data;

        // rebuild cache map
        constexpr static auto array_time_slot = std::to_array(
            {
                WeatherTimeSlot::REALTIME,
                WeatherTimeSlot::TODAY,
                WeatherTimeSlot::TOMMROW,
                WeatherTimeSlot::DAY_AFTER_TOMMROW,
            });

        constexpr static auto array_weather_item = std::to_array(
            {
                WeatherItem::TEMPERATURE,
                WeatherItem::WEATHER_TEXT,
                WeatherItem::WEATHER_CODE,
                WeatherItem::HUMIDITY,
                WeatherItem::WIND,
                WeatherItem::AIR_QUALITY,
                WeatherItem::AIR_PM2P5,
                WeatherItem::AIR_PM10,
                WeatherItem::UV_INDEX,
                WeatherItem::ALERTS,
                WeatherItem::PRECIPITATION,
            });

        for (auto time_slot : array_time_slot) {
            for (auto item : array_weather_item) {
                if (const auto item_str = weather_data->getWeatherItem(time_slot, item);
                    !item_str.empty()) {
                    auto item_wstr = cmn::MultiByte2WideChar(item_str.c_str());
                    new_snapshot->cache_map.emplace(WeatherDataKey{ .time_slot = time_slot, .item = item },
                                                    item_wstr);
                }
            }
        }

        // rebuild tooltip text
        const auto summary_wstr = cmn::MultiByte2WideChar(weather_data->getWeatherSummary().c_str());
        new_snapshot->tooltip_text = std::format(
            L"{} {}", FormatLocation(current_loc_, config_.format_geo_coords_in_summary), summary_wstr
        );
    }

    cache_snapshot.store(new_snapshot, std::memory_order_release);
}

std::shared_ptr<DataManager::Snapshot> DataManager::GetSnapshot() {
    return cache_snapshot.load(std::memory_order_acquire);
}

const DataManager::Config& DataManager::GetConfig() const {
    return config_;
}

void DataManager::SetConfig(const Config &cfg) {
    config_ = cfg;
}
