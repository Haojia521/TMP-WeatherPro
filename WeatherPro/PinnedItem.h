#pragma once

#include "PluginInterface.h"

#include <WPCore/DataDef.h>

class PinnedItem : public IPluginItem
{
public:
    PinnedItem(WeatherTimeSlot wts, WeatherItem wi);

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;

    int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;

private:
    WeatherTimeSlot time_slot;
    WeatherItem weather_item;

    std::wstring name_text;
    std::wstring label_text;
    std::wstring id_text;
};
