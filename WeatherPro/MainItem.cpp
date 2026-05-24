#include "pch.h"
#include "WeatherPro.h"
#include "MainItem.h"
#include "DataManager.h"
#include "MainSettingsDlg.h"
#include "resource.h"
#include "Common.h"

#include <mutex>
#include <memory>

namespace 
{
    int CalcPixelSize(int dpi, int pixel_size) {
        return dpi * pixel_size / 96;
    }

    constexpr int TS_GAP_PIXELS{ 10 };             // pixels between the end and the beginning when scrolling
    constexpr int TS_PIXELS_PER_SECOND{ 30 };      // pixels scrolled per second
    constexpr DWORD TS_INITIAL_PAUSE_MS{ 1000 };   // pause duration (in milliseconds) before each round of scrolling begins
    constexpr DWORD TS_STOP_PAUSE_MS{ 2000 };      // pause duration (in milliseconds) after each round of scrolling end
    constexpr int TS_LOOPS_BEFORE_STOP{ 2 };       // number loops of each round of scrolling

    struct TextScrollingState
    {
        CString text;

        int text_width = 0;
        int text_height = 0;

        bool need_scroll = false;
        bool metrics_valid = false;

        CSize last_draw_size{ 0, 0 };

        QWORD start_tick = 0;
    };

    TextScrollingState text_scrolling_state;

    void SetText(const CString &text) {
        if (text_scrolling_state.text == text)
            return;

        text_scrolling_state.text = text;

        text_scrolling_state.text_width = 0;
        text_scrolling_state.text_height = 0;

        text_scrolling_state.need_scroll = false;
        text_scrolling_state.metrics_valid = false;

        text_scrolling_state.last_draw_size = CSize(0, 0);
        text_scrolling_state.start_tick = ::GetTickCount64();
    }

    int CalcOffset() {
        if (!text_scrolling_state.need_scroll)
            return 0;

        const int reset_width =
            text_scrolling_state.text_width + TS_GAP_PIXELS;

        if (reset_width <= 0)
            return 0;

        const auto now = ::GetTickCount64();
        const auto elapsed = now - text_scrolling_state.start_tick;

        const auto one_loop_ms = static_cast<QWORD>(
            static_cast<long long>(reset_width) * 1000 / TS_PIXELS_PER_SECOND
            );

        const auto moving_phase_ms = one_loop_ms * TS_LOOPS_BEFORE_STOP;

        const auto total_cycle_ms = TS_INITIAL_PAUSE_MS + moving_phase_ms + TS_STOP_PAUSE_MS;

        if (total_cycle_ms == 0)
            return 0;

        const auto cycle_elapsed = elapsed % total_cycle_ms;

        // 每个大周期开始时，先完整显示一会儿
        if (cycle_elapsed < TS_INITIAL_PAUSE_MS)
            return 0;

        const auto moving_elapsed =
            cycle_elapsed - TS_INITIAL_PAUSE_MS;

        // 滚动 N 圈结束后，停在初始位置几秒
        if (moving_elapsed >= moving_phase_ms)
            return 0;

        int offset = static_cast<int>(
            static_cast<long long>(moving_elapsed) * TS_PIXELS_PER_SECOND / 1000
            );

        offset %= reset_width;

        return offset;
    }

    void UpdateMetrics(const CDC &dc, const CRect &rc) {
        const CSize draw_size = rc.Size();

        if (text_scrolling_state.metrics_valid &&
            text_scrolling_state.last_draw_size == draw_size)
        {
            return;
        }

        const CSize text_size = dc.GetTextExtent(text_scrolling_state.text);

        text_scrolling_state.text_width = text_size.cx;
        text_scrolling_state.text_height = text_size.cy;

        text_scrolling_state.need_scroll = text_scrolling_state.text_width > rc.Width();

        text_scrolling_state.last_draw_size = draw_size;
        text_scrolling_state.metrics_valid = true;

        text_scrolling_state.start_tick = ::GetTickCount64();
    }

    void DrawTextScrolling(CDC &dc, const CRect &rc, bool dark_mode) {
        if (text_scrolling_state.text.IsEmpty() || rc.IsRectEmpty())
            return;

        UpdateMetrics(dc, rc);

        const CString text = text_scrolling_state.text;
        const int text_width = text_scrolling_state.text_width;
        const int text_height = text_scrolling_state.text_height;
        const bool need_scroll = text_scrolling_state.need_scroll;

        dc.SaveDC();

        dc.IntersectClipRect(rc);
        dc.SetBkMode(TRANSPARENT);

        dc.SetTextColor(
            dark_mode ? RGB(230, 230, 230) : RGB(30, 30, 30)
        );

        const int text_y = rc.top + (rc.Height() - text_height) / 2;

        if (!need_scroll) {
            dc.TextOut(rc.left + (rc.Width() - text_width), text_y, text);
            dc.RestoreDC(-1);
            return;
        }

        const int offset_x = CalcOffset();
        //const int text_x = rc.left - offset_x;
        const int base_x = rc.right - text_width;
        const int text_x = base_x - offset_x;

        dc.TextOut(text_x, text_y, text);

        dc.TextOut(
            text_x + text_width + TS_GAP_PIXELS,
            text_y,
            text
        );

        dc.RestoreDC(-1);
    }

    // draw a red dot on icon upper-right corner
    void DrawNotificationDot(CDC &dc, const CRect &icon_rc)
    {
        auto icon_size = std::min(icon_rc.Height(), icon_rc.Width());
        auto dot_size = icon_size / 3;

        CRect dot_rc{
            icon_rc.right - dot_size,
            icon_rc.top,
            icon_rc.right,
            icon_rc.top + dot_size,
        };

        // create & select a red brush
        CBrush red_brush(RGB(238, 64, 76));
        CBrush* old_brush = dc.SelectObject(&red_brush);

        // create & select a red pen
        CPen red_pen(PS_SOLID, 1, RGB(238, 64, 76));
        CPen* old_pen = dc.SelectObject(&red_pen);

        // draw the dot
        dc.Ellipse(dot_rc);

        // restore the objects
        dc.SelectObject(old_brush);
        dc.SelectObject(old_pen);
    }

    class CBorrowedCDC
    {
    public:
        explicit CBorrowedCDC(HDC hdc)
            : m_hdc(hdc)
        {
            if (m_hdc != nullptr)
            {
                m_saved = ::SaveDC(m_hdc);
                m_dc.Attach(m_hdc);
            }
        }

        ~CBorrowedCDC()
        {
            if (m_hdc != nullptr && m_saved != 0)
            {
                ::RestoreDC(m_hdc, m_saved);
            }

            if (m_dc.GetSafeHdc() != nullptr)
            {
                m_dc.Detach();
            }
        }

        CBorrowedCDC(const CBorrowedCDC&) = delete;
        CBorrowedCDC& operator=(const CBorrowedCDC&) = delete;

        CDC& get()
        {
            return m_dc;
        }

        CDC* get_ptr()
        {
            return &m_dc;
        }

        bool valid() const
        {
            return m_hdc != nullptr && m_dc.GetSafeHdc() != nullptr;
        }

    private:
        HDC m_hdc{};
        int m_saved{};
        CDC m_dc;
    };
}

const wchar_t* MainItem::GetItemName() const {
    return L"WeatherPro";
}

const wchar_t* MainItem::GetItemId() const {
    return L"QvgpuGO5";
}

const wchar_t* MainItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* MainItem::GetItemValueText() const
{
    return L"";
}

const wchar_t* MainItem::GetItemValueSampleText() const {
    return L"";
}

bool MainItem::IsCustomDraw() const {
    return true;
}

int MainItem::GetItemWidth() const {
    // 60 pixels under 96 dpi
    return 60;
}

int MainItem::GetItemWidthEx(void* hDC) const {
    const auto &cfg = DataManager::Instance().GetConfig();

    if (!cfg.main_item_scroll_text) {
        HDC raw_dc = static_cast<HDC>(hDC);
        if (raw_dc == nullptr)
            return 0;

        const auto data_snapshot = DataManager::GetSnapshot();
        if (data_snapshot == nullptr) {
            return 0;
        }

        CString text;
        int icon_width{ 0 };
        if (cfg.draw_weather_icon) {
            icon_width = CalcPixelSize(taskbar_wnd_dpi, 20);
            text = data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::TEMPERATURE);
        } else {
            text.Format(L"%s %s",
                        data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::WEATHER_TEXT),
                        data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::TEMPERATURE));
        }

        SIZE size{};
        if (!::GetTextExtentPoint32(raw_dc, text, text.GetLength(), &size)) {
            return 0;
        }
        auto text_width = size.cx;

        return text_width + icon_width;
    }

    return 0;
}

void MainItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) {
    if (hDC == nullptr || w <= 0 || h <= 0)
        return;

    const auto is_updating = DataManager::IsUpdating();
    const auto data_snapshot = DataManager::GetSnapshot();
    if (!is_updating && !(data_snapshot != nullptr && data_snapshot->GetWeatherData() != nullptr)) {
        return;
    }

    CBorrowedCDC borrowed_dc{ static_cast<HDC>(hDC) };

    CRect rc_text(x, y, x + w, y + h);

    const auto &api = DataManager::Instance().GetApi();
    const auto &cfg = DataManager::Instance().GetConfig();
    CString text;
    
    // draw weather icon
    if (cfg.draw_weather_icon) {
        // adjust rects for icon and text
        const auto icon_size = CalcPixelSize(taskbar_wnd_dpi, 16);
        const auto icon_text_gap = CalcPixelSize(taskbar_wnd_dpi, 4);

        auto icon_y = y + ((h - icon_size) > 0 ? (h - icon_size) / 2 : 0);
        CRect rc_icon(x, icon_y, x + icon_size, icon_y + icon_size);
        rc_text.left = rc_icon.right + icon_text_gap;

        if (is_updating) {
            if (auto icon_res = IconSheetManager::Instance().GetIconSheet(IconResType::Loading);
                icon_res != nullptr) {
                static int loading_frame_idx{ 0 };
                
                icon_res->Draw(borrowed_dc.get_ptr(), rc_icon, loading_frame_idx);
                loading_frame_idx = (++loading_frame_idx) % icon_res->GetMaxCount();
            }

            text = cmn::GetStringRes(IDS_UPDATING);
        } else {
            // only draw text of temperature
            text = data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::TEMPERATURE);

            if (const auto *icon_res = api.GetWeatherIcons();
                icon_res != nullptr) {
                auto weather_code = data_snapshot->GetWeatherData()->getWeatherItem(cfg.time_slot, WeatherItem::WEATHER_CODE);
                icon_res->Draw(borrowed_dc.get_ptr(), rc_icon, api.GetWeatherIconIndex(weather_code));
            }

            if (cfg.draw_alerts_notification_dot &&
                !data_snapshot->GetWeatherData()->getWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::ALERTS).empty()) {
                DrawNotificationDot(borrowed_dc.get(), rc_icon);
            }
        }
    } else {
        if (is_updating) {
            text = cmn::GetStringRes(IDS_UPDATING);
        } else {
            text.Format(L"%s %s",
                        data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::WEATHER_TEXT),
                        data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::TEMPERATURE));
        }
    }

    // draw text
    if (cfg.main_item_scroll_text) {
        SetText(text);
        DrawTextScrolling(borrowed_dc.get(), rc_text, dark_mode);
    } else {
        SetText(CString{});
        auto dt_flag = DT_VCENTER;
        if (text_align_right) {
            dt_flag |= DT_RIGHT;
        }
        borrowed_dc.get().DrawTextW(text, rc_text, dt_flag);
    }
}

int MainItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) {
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
