#include "pch.h"
#include "WeatherPro.h"
#include "MainItem.h"
#include "DataManager.h"
#include "MainSettingsDlg.h"
#include "resource.h"
#include "Common.h"

#include <mutex>
#include <memory>
#include <optional>

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

    class ScrollableText
    {
    public:
        void SetText(const CString &text) {
            if (scrolling_state_.text == text)
                return;

            scrolling_state_.text = text;

            scrolling_state_.text_width = 0;
            scrolling_state_.text_height = 0;

            scrolling_state_.need_scroll = false;
            scrolling_state_.metrics_valid = false;

            scrolling_state_.last_draw_size = CSize(0, 0);
            scrolling_state_.start_tick = ::GetTickCount64();
        }

        void Draw(CDC &dc, const CRect &rc, bool text_align_right) {
            if (scrolling_state_.text.IsEmpty() || rc.IsRectEmpty())
                return;

            UpdateMetrics(dc, rc);

            const CString text = scrolling_state_.text;
            const int text_width = scrolling_state_.text_width;
            const int text_height = scrolling_state_.text_height;
            const bool need_scroll = scrolling_state_.need_scroll;

            dc.SaveDC();

            dc.IntersectClipRect(rc);
            dc.SetBkMode(TRANSPARENT);

            const int text_y = rc.top + (rc.Height() - text_height) / 2;

            if (!need_scroll) {
                if (text_align_right) {
                    dc.TextOutW(rc.left + (rc.Width() - text_width), text_y, text);
                } else {
                    dc.TextOutW(rc.left, text_y, text);
                }
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

    private:
        struct TextScrollingState
        {
            CString text;

            int text_width = 0;
            int text_height = 0;

            CSize last_draw_size{ 0, 0 };
            QWORD start_tick = 0;

            bool need_scroll = false;
            bool metrics_valid = false;
        };

        [[nodiscard]]
        int CalcOffset() const {
            if (!scrolling_state_.need_scroll)
                return 0;

            const int reset_width =
                scrolling_state_.text_width + TS_GAP_PIXELS;

            if (reset_width <= 0)
                return 0;

            const auto now = ::GetTickCount64();
            const auto elapsed = now - scrolling_state_.start_tick;

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

            if (scrolling_state_.metrics_valid &&
                scrolling_state_.last_draw_size == draw_size)
            {
                return;
            }

            const CSize text_size = dc.GetTextExtent(scrolling_state_.text);

            scrolling_state_.text_width = text_size.cx;
            scrolling_state_.text_height = text_size.cy;

            scrolling_state_.need_scroll = scrolling_state_.text_width > rc.Width();

            scrolling_state_.last_draw_size = draw_size;
            scrolling_state_.metrics_valid = true;

            scrolling_state_.start_tick = ::GetTickCount64();
        }

        TextScrollingState scrolling_state_;
    };

    enum class IconKind {
        None,
        Loading,
        Weather
    };

    struct IconModel {
        IconKind kind{ IconKind::None };
        int weather_icon_index{ 0 };
    };

    struct TextModel {
        CString line1;
        CString line2;
        bool scroll{ false };
    };

    struct RenderModel {
        TextModel text;
        IconModel icon;
        bool draw_notification_dot{ false };
    };

    struct LayoutResult {
        bool dual_line{ false };

        std::optional<CRect> icon_rect;
        std::array<CRect, 2> text_rects;
    };

    RenderModel BuildRenderModel(bool is_updating, bool dual_line,
                                 const DataManager::Snapshot* data_snapshot,
                                 const WeatherApi& api,
                                 const DataManager::Config& cfg) {
        RenderModel model;

        if (is_updating) {
            model.text.line1 = dual_line ? CString{} : cmn::GetStringRes(IDS_UPDATING);
            model.text.line2 = dual_line ? cmn::GetStringRes(IDS_UPDATING) : CString{};

            if (cfg.draw_weather_icon) {
                model.icon.kind = IconKind::Loading;
            }

            return model;
        }

        if (dual_line) {
            model.text.line1 = data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::WEATHER_TEXT);
            model.text.line2 = data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::TEMPERATURE);
        } else {
            if (cfg.draw_weather_icon) {
                model.text.line1 = data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::TEMPERATURE);
            } else {
                model.text.line1.Format(
                    L"%s %s",
                    data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::WEATHER_TEXT),
                    data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::TEMPERATURE)
                );
            }
        }

        if (cfg.draw_weather_icon) {
            if (const auto *weather_icons = api.GetWeatherIcons();
                weather_icons != nullptr) {
                const std::wstring weather_code{
                    data_snapshot->GetWeatherItem(cfg.time_slot, WeatherItem::WEATHER_CODE)
                };

                model.icon.kind = IconKind::Weather;
                model.icon.weather_icon_index = api.GetWeatherIconIndex(weather_code);
            }
        }

        model.draw_notification_dot =
            cfg.draw_weather_icon &&
            cfg.draw_alerts_notification_dot &&
            !std::wstring_view{
                data_snapshot->GetWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::ALERTS)
            }.empty();

        model.text.scroll = cfg.main_item_scroll_text;

        return model;
    }

    LayoutResult BuildLayout(int x, int y, int w, int h, bool dual_line, bool has_icon, int taskbar_wnd_dpi) {
        LayoutResult layout;
        layout.dual_line = dual_line;

        const auto icon_text_gap = CalcPixelSize(taskbar_wnd_dpi, 4);
        const auto pixel_size_16 = CalcPixelSize(taskbar_wnd_dpi, 16);
        const auto pixel_size_32 = CalcPixelSize(taskbar_wnd_dpi, 32);

        int text_left = x;

        if (has_icon) {
            const auto icon_size = dual_line ? pixel_size_32 : pixel_size_16;
            const auto icon_y = y + std::max(0, h - icon_size) / 2;

            CRect rc_icon{
                x,
                icon_y,
                x + icon_size,
                icon_y + icon_size
            };

            layout.icon_rect = rc_icon;
            text_left = rc_icon.right + icon_text_gap;
        }

        if (dual_line) {
            const auto text_y_offset = h / 2 - pixel_size_16;

            CRect rc_line1{
                text_left,
                y + text_y_offset,
                x + w,
                y + text_y_offset + pixel_size_16
            };

            CRect rc_line2{
                text_left,
                rc_line1.bottom + 1,
                x + w,
                rc_line1.bottom + pixel_size_16 + 1
            };

            layout.text_rects[0] = rc_line1;
            layout.text_rects[1] = rc_line2;
        } else {
            CRect rc_text{
                text_left,
                y,
                x + w,
                y + h
            };

            layout.text_rects[0] = rc_text;
        }

        return layout;
    }

    void DrawIcon(CDC& dc, const CRect& rc_icon, const IconModel& icon, const WeatherApi& api) {
        static int loading_frame_idx{ 0 };

        switch (icon.kind) {
            case IconKind::None:
                return;

            case IconKind::Loading: {
                const auto *icon_res = IconSheetManager::Instance().GetIconSheet(IconResType::Loading);
                if (icon_res == nullptr) {
                    return;
                }

                icon_res->Draw(&dc, rc_icon, loading_frame_idx);
                loading_frame_idx = (loading_frame_idx + 1) % icon_res->GetMaxCount();
                return;
            }

            case IconKind::Weather: {
                const auto* icon_res = api.GetWeatherIcons();
                if (icon_res == nullptr) {
                    return;
                }

                icon_res->Draw(&dc, rc_icon, icon.weather_icon_index);
                return;
            }
        }
    }

    ScrollableText scrollable_text_line1, scrollable_text_line2;

    void DrawTexts(CDC& dc, const TextModel& text, const LayoutResult& layout,
                   bool text_align_right) {
        auto dt_flag = DT_VCENTER | DT_SINGLELINE;

        if (text_align_right) {
            dt_flag |= DT_RIGHT;
        }

        if (layout.dual_line) {
            CRect rc_line1 = layout.text_rects[0];
            CRect rc_line2 = layout.text_rects[1];

            if (text.scroll) {
                scrollable_text_line1.SetText(text.line1);
                scrollable_text_line1.Draw(dc, rc_line1, text_align_right);
                
                scrollable_text_line2.SetText(text.line2);
                scrollable_text_line2.Draw(dc, rc_line2, text_align_right);
            } else {
                dc.DrawTextW(text.line1, rc_line1, dt_flag);
                dc.DrawTextW(text.line2, rc_line2, dt_flag);
            }
        } else {
            CRect rc_text = layout.text_rects[0];

            if (text.scroll) {
                scrollable_text_line1.SetText(text.line1);
                scrollable_text_line1.Draw(dc, rc_text, text_align_right);

                scrollable_text_line2.SetText(CString{});
            }else {
                dc.DrawTextW(text.line1, rc_text, dt_flag);
            }
        }
    }

    // draw a red dot on icon upper-right corner
    void DrawNotificationDot(CDC &dc, const CRect &icon_rc)
    {
        const auto icon_size = std::min(icon_rc.Height(), icon_rc.Width());
        const auto dot_size = icon_size / 3;

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
        CBorrowedCDC(CBorrowedCDC&&) = delete;
        CBorrowedCDC& operator=(const CBorrowedCDC&) = delete;
        CBorrowedCDC& operator=(CBorrowedCDC&&) = delete;

        CDC& get()
        {
            return m_dc;
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
    return dual_line_mode ? 80 : 60;
}

int MainItem::GetItemWidthEx(void* hDC) const {
    if (const auto &cfg = DataManager::Instance().GetConfig();
        !cfg.main_item_scroll_text) {
        return real_width;
    }
    
    auto max_width = CalcPixelSize(taskbar_wnd_dpi, GetItemWidth());
    return std::min(max_width, real_width);
}

void MainItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) {
    if (hDC == nullptr || w <= 0 || h <= 0) {
        return;
    }

    const auto is_updating = DataManager::IsUpdating();
    const auto data_snapshot = DataManager::GetSnapshot();

    if (!is_updating && data_snapshot == nullptr) {
        return;
    }

    CBorrowedCDC borrowed_dc{ static_cast<HDC>(hDC) };

    const auto& api = DataManager::Instance().GetApi();
    const auto& cfg = DataManager::Instance().GetConfig();

    const auto pixel_size_32 = CalcPixelSize(taskbar_wnd_dpi, 32);

    dual_line_mode = cfg.enable_dual_line_mode && h >= pixel_size_32;

    const auto *snapshot_ptr = data_snapshot.get();
    auto model = BuildRenderModel(is_updating, dual_line_mode, snapshot_ptr, api, cfg);

    const bool has_icon = model.icon.kind != IconKind::None;
    auto layout = BuildLayout(x, y, w, h, dual_line_mode, has_icon, taskbar_wnd_dpi);

    real_width = std::max(borrowed_dc.get().GetTextExtent(model.text.line1).cx,
                          borrowed_dc.get().GetTextExtent(model.text.line2).cx);

    if (layout.icon_rect.has_value()) {
        DrawIcon(borrowed_dc.get(), *layout.icon_rect, model.icon, api);

        if (model.draw_notification_dot) {
            DrawNotificationDot(borrowed_dc.get(), *layout.icon_rect);
        }

        real_width += layout.text_rects[0].left - layout.icon_rect.value().left;
    }

    DrawTexts(borrowed_dc.get(), model.text, layout, text_align_right);
}

int MainItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) {
    AFX_MANAGE_STATE(AfxGetStaticModuleState())

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
