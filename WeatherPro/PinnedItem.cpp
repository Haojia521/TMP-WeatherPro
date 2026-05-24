#include "pch.h"
#include "WeatherPro.h"
#include "resource.h"
#include "PinnedItem.h"
#include "Common.h"
#include "DataManager.h"

#include <format>

namespace
{
    std::wstring ConstructItemLabel(WeatherTimeSlot time_slot, WeatherItem item) {
        std::wstring name;

        auto str_time_slot = cmn::GetString(time_slot);
        if (time_slot == WeatherTimeSlot::REALTIME) {
            str_time_slot.Empty();
        }
        if (auto pos = str_time_slot.Find(L'|'); pos >= 0) {
            str_time_slot = str_time_slot.Left(pos).Trim();
        }

        auto str_item = cmn::GetString(item);
        if (auto pos = str_item.Find(L'|'); pos >= 0) {
            str_item = str_item.Left(pos).Trim();
        }

        if (time_slot == WeatherTimeSlot::REALTIME) {
            name = std::format(L"{}", str_item.GetString());
        } else {
            name = std::format(L"{} {}", str_time_slot.GetString(), str_item.GetString());
        }

        if (str_time_slot.IsEmpty() && str_item.IsEmpty()) {
            name = std::format(L"?{}?{}?", static_cast<int>(time_slot), static_cast<int>(item));
        }

        return name;
    }

    std::wstring ConstructItemId(WeatherTimeSlot time_slot, WeatherItem item) {
        auto int_to_wstring_8 = [](int value) {
            static constexpr wchar_t chars[] =
                L"0123456789"
                L"abcdefghijklmnopqrstuvwxyz"
                L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

            static constexpr std::uint64_t BASE = 62;
            static constexpr std::uint64_t MOD = 218340105584896ULL; // 62^8

            // 将 int 映射到 uint32_t，保证负数和正数也能区分
            std::uint64_t x = static_cast<std::uint32_t>(value);

            // 做一个确定性的扰动，让结果看起来更“随机”
            // 乘数需要和 MOD 互质，这里选一个奇数且不被 31 整除的数
            x = (x * 11400714819323198485ULL + 123456789ULL) % MOD;

            std::wstring result(8, L'0');

            for (int i = 7; i >= 0; --i) {
                result[i] = chars[x % BASE];
                x /= BASE;
            }

            return result;
        };

        return int_to_wstring_8((static_cast<int>(time_slot) << 16) | static_cast<int>(item));
    }

    //const wchar_t* GetItemValueSampleText(WeatherTimeSlot time_slot, WeatherItem item) {
    //    const auto api_type = DataManager::Instance().GetConfig().api_type;

    //    if (item == WeatherItem::TEMPERATURE) {
    //        if (time_slot == WeatherTimeSlot::REALTIME) {
    //            return L"-20.0℃";
    //        } else {
    //            return L"-18.0~20.0℃";
    //        }
    //    } else if (item == WeatherItem::WEATHER_TEXT) {
    //        if (time_slot == WeatherTimeSlot::REALTIME) {
    //            if (api_type == ApiType::WeatherComCnSpider || api_type == ApiType::QWeather) {
    //                return L"大到暴雨";
    //            } else {
    //                return L"thunderstorm";
    //            }
    //        } else {
    //            if (api_type == ApiType::WeatherComCnSpider || api_type == ApiType::QWeather) {
    //                return L"多云~暴雨";
    //            } else {
    //                return L"thunderstorm";
    //            }
    //        }
    //    } else if (item == WeatherItem::HUMIDITY) {
    //        return L"100 %";
    //    } else if (item == WeatherItem::WIND) {
    //        if (api_type == ApiType::WeatherComCnSpider || api_type == ApiType::QWeather) {
    //            return L"东北风 12km/h";
    //        } else {
    //            return L"NE 12km/h";
    //        }
    //    } else if (item == WeatherItem::AIR_QUALITY) {
    //        if (api_type == ApiType::WeatherComCnSpider) {
    //            return L"999";
    //        } else if (api_type == ApiType::QWeather) {
    //            return L"Good (AQI (CN): 100)";
    //        } else if (api_type == ApiType::OpenWeather) {
    //            return L"99";
    //        }
    //    } else if (item == WeatherItem::AIR_PM2P5) {
    //        if (api_type == ApiType::WeatherComCnSpider) {
    //            return L"999";
    //        } else if (api_type == ApiType::QWeather || api_type == ApiType::OpenWeather) {
    //            return L"100.00μg/m3";
    //        }
    //    } else if (item == WeatherItem::AIR_PM10) {
    //        if (api_type == ApiType::WeatherComCnSpider) {
    //            return L"999";
    //        } else if (api_type == ApiType::QWeather || api_type == ApiType::OpenWeather) {
    //            return L"100.00μg/m3";
    //        }
    //    } else if (item == WeatherItem::UV_INDEX) {
    //        return L"10";
    //    } else if (item == WeatherItem::PRECIPITATION) {
    //        if (time_slot == WeatherTimeSlot::REALTIME) {
    //            if (api_type == ApiType::OpenWeather) {
    //                return L"rain 100mm/h";
    //            }
    //        } else {
    //            if (api_type == ApiType::OpenWeather) {
    //                return L"100% rain 100mm";
    //            }
    //        }
    //    }

    //    return L"???";
    //}
}

PinnedItem::PinnedItem(WeatherTimeSlot wts, WeatherItem wi) : time_slot(wts), weather_item(wi) {
    auto label = ConstructItemLabel(wts, wi);

    name_text = std::format(L"WeatherPro-{}", label);
    id_text = ConstructItemId(wts, wi);
    label_text = std::format(L"{}: ", label);
}

const wchar_t* PinnedItem::GetItemName() const {
    return name_text.c_str();
}

const wchar_t* PinnedItem::GetItemId() const {
    return id_text.c_str();
}

const wchar_t* PinnedItem::GetItemLableText() const {
    return label_text.c_str();
}

const wchar_t* PinnedItem::GetItemValueSampleText() const {
    return GetItemValueText();
}

const wchar_t* PinnedItem::GetItemValueText() const {
    const auto data_snapshot = DataManager::GetSnapshot();
    if (data_snapshot == nullptr) {
        return L"--";
    }

    return data_snapshot->GetWeatherItem(time_slot, weather_item);
}

int PinnedItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    if (type == MouseEventType::MT_DBCLICKED) {
        const auto &cfg = DataManager::Instance().GetConfig();

        if (cfg.double_click_action == DataManager::LDoubleClickAction::OpenSettingWindow) {
            WeatherPro::Instance().ShowOptionsDialog(hWnd);
        } else {
            WeatherPro::Instance().UpdateWeather(true);
        }

        return 1;
    }

    return 0;
}

