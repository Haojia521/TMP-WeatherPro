#pragma once

#include "PluginInterface.h"

class MainItem : public IPluginItem
{
public:
    virtual ~MainItem() = default;

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;
    bool IsCustomDraw() const override;
    int GetItemWidth() const override;
    int GetItemWidthEx(void* hDC) const override;
    void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;
    int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;

    void SetTaskbarWndDPI(int dpi) { taskbar_wnd_dpi = dpi; }
    void SetTextAlignRight(bool flag = true) { text_align_right = flag; }

private:
    int taskbar_wnd_dpi{ 96 };
    bool text_align_right{ true };
    bool dual_line_mode{ false };
    int real_width{ 0 };
};
