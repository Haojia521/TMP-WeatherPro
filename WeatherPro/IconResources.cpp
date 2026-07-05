#include "pch.h"
#include "resource.h"
#include "IconResources.h"

#include <algorithm>
#include <array>

#include <gdiplus.h>

////////////////////////////////////////////////////////////////////////////////

namespace
{
    std::unique_ptr<Gdiplus::Bitmap> LoadPngFromResource(UINT resourceId) {
        AFX_MANAGE_STATE(AfxGetStaticModuleState())

            HINSTANCE hResInst = AfxGetResourceHandle();
        if (hResInst == nullptr)
            return nullptr;

        HRSRC hResInfo = ::FindResource(hResInst, MAKEINTRESOURCE(resourceId), _T("PNG"));
        if (hResInfo == nullptr)
            return nullptr;

        const DWORD resourceSize = ::SizeofResource(hResInst, hResInfo);
        if (resourceSize == 0)
            return nullptr;

        HGLOBAL hResData = ::LoadResource(hResInst, hResInfo);
        if (hResData == nullptr)
            return nullptr;

        const void* pResData = ::LockResource(hResData);
        if (pResData == nullptr)
            return nullptr;

        HGLOBAL hBuffer = ::GlobalAlloc(GMEM_MOVEABLE, resourceSize);
        if (hBuffer == nullptr)
            return nullptr;

        void* pBuffer = ::GlobalLock(hBuffer);
        if (pBuffer == nullptr)
        {
            ::GlobalFree(hBuffer);
            return nullptr;
        }

        ::CopyMemory(pBuffer, pResData, resourceSize);
        ::GlobalUnlock(hBuffer);

        IStream* pStream = nullptr;
        const HRESULT hr = ::CreateStreamOnHGlobal(hBuffer, TRUE, &pStream);
        if (FAILED(hr) || pStream == nullptr)
        {
            ::GlobalFree(hBuffer);
            return nullptr;
        }

        std::unique_ptr<Gdiplus::Bitmap> bitmap(Gdiplus::Bitmap::FromStream(pStream, FALSE));
        pStream->Release();

        if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
            bitmap.reset();
        }

        return bitmap;
    }

    struct SubSheetMeta
    {
        int cell_size{ 0 };
        int offset_x{ 0 };
        int offset_y{ 0 };
    };

    using SubShteetMetaList = std::vector<SubSheetMeta>;

    class CompositedIconSheet : public IconSheet
    {
    public:
        ~CompositedIconSheet() override {
            DeleteObject(hbitmap_);
        }

        bool Load(const UINT resource_id, const int cols, const int rows, const int margin,
                  SubShteetMetaList &&sub_meta_list) {
            if (hbitmap_ != nullptr) {
                return true;
            }

            if (cols <= 0 || rows <= 0 || margin < 0 || sub_meta_list.empty()) {
                TRACE(L"invalid cols/rows/margin/sub_meta_list\n");
                return false;
            }
            
            const auto gdiplus_bitmap = LoadPngFromResource(resource_id);
            if (gdiplus_bitmap == nullptr) {
                TRACE(L"failed to load png resources (id: %u)", resource_id);
                return false;
            }

            const auto image_w = static_cast<int>(gdiplus_bitmap->GetWidth());
            const auto image_h = static_cast<int>(gdiplus_bitmap->GetHeight());

            {
                int img_max_x = 0;
                int img_max_y = 0;
                for (const auto [cell_size, offset_x, offset_y] : sub_meta_list) {
                    auto max_x = offset_x + margin + (cell_size + margin) * cols;
                    auto max_y = offset_y + margin + (cell_size + margin) * rows;

                    img_max_x = std::max(img_max_x, max_x);
                    img_max_y = std::max(img_max_y, max_y);
                }

                if (img_max_x > image_w || img_max_y > image_h) {
                    TRACE(L"sub-sheets (max x*y: %d*%d) exceed the size of the image (%d*%d)",
                          img_max_x, img_max_y, image_w, image_h);
                    return false;
                }
            }

            if (gdiplus_bitmap->GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbitmap_) != Gdiplus::Ok) {
                TRACE(L"failed to get hbitmap from gdiplus bitmap");
                hbitmap_ = nullptr;
                return false;
            }

            cols_ = cols;
            rows_ = rows;
            margin_ = margin;
            sub_sheets_ = std::move(sub_meta_list);

            return true;
        }

        bool Draw(CDC &dc, const CRect &dest_rect, int index) const override {
            if (hbitmap_ == nullptr || index < 0 || index >= GetMaxCount())
                return false;

            const int row = index / cols_;
            const int col = index % cols_;

            return Draw(dc, dest_rect, row, col);
        }

        bool Draw(CDC &dc, const CRect &dest_rect, int row, int col) const override {
            if (hbitmap_ == nullptr || sub_sheets_.empty()) {
                return false;
            }

            if (dest_rect.Width() <= 0 || dest_rect.Height() <= 0)
                return false;

            if (row < 0 || row >= rows_ || col < 0 || col >= cols_)
                return false;

            const int target_size = std::min(dest_rect.Width(), dest_rect.Height());
            auto itr = std::ranges::upper_bound(
                sub_sheets_,
                target_size,
                std::less<>{},
                &SubSheetMeta::cell_size
            );

            const auto [cell_size, offset_x, offset_y] = 
                itr == sub_sheets_.begin() ? sub_sheets_.front() : *(--itr);

            const int src_x = offset_x + margin_ + col * (cell_size + margin_);
            const int src_y = offset_y + margin_ + row * (cell_size + margin_);

            const HDC hdc = ::CreateCompatibleDC(dc.GetSafeHdc());
            if (!hdc)
                return false;

            const HGDIOBJ oldObj = ::SelectObject(hdc, hbitmap_);

            constexpr BLENDFUNCTION blend {
                .BlendOp = AC_SRC_OVER,
                .BlendFlags = 0,
                .SourceConstantAlpha = 255,
                .AlphaFormat = AC_SRC_ALPHA
            };

            const BOOL ok = ::AlphaBlend(
                dc.GetSafeHdc(),
                dest_rect.left,
                dest_rect.top,
                dest_rect.Width(),
                dest_rect.Height(),
                hdc,
                src_x,
                src_y,
                cell_size,
                cell_size,
                blend
            );

            ::SelectObject(hdc, oldObj);
            ::DeleteDC(hdc);

            return ok != FALSE;
        }

        int GetCols() const override {
            return cols_;
        }

        int GetRows() const override {
            return rows_;
        }

        int GetMaxCount() const override {
            return cols_ * rows_;
        }

    private:
        SubShteetMetaList sub_sheets_;
        HBITMAP hbitmap_{ nullptr };
        int cols_{ 0 };
        int rows_{ 0 };
        int margin_{ 0 };
    };
}

////////////////////////////////////////////////////////////////////////////////

/**********************************************************************************************************************/
// IconSheetManager

IconManager& IconManager::Instance() {
    static IconManager instance;

    return instance;
}

const IconSheet* IconManager::GetIconSheet(IconResType type) {
    if (icon_sheets_.contains(type)) {
        return icon_sheets_[type].get();
    } else {
        return nullptr;
    }
}

HICON IconManager::GetIcon(UINT icon_id) {
    if (!icons_.contains(icon_id)) {
        AFX_MANAGE_STATE(AfxGetStaticModuleState())

        const auto hicon = static_cast<HICON>(
            LoadImageW(AfxGetInstanceHandle(), MAKEINTRESOURCE(icon_id), IMAGE_ICON,
                       16, 16, 0));

        icons_[icon_id] = hicon;
    }

    return icons_[icon_id];
}

void IconManager::LoadIconResources() {
    AFX_MANAGE_STATE(AfxGetStaticModuleState())

    // start gdi+ temporarily
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;

    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok)
        return;

    constexpr std::array<SubSheetMeta, 9> SSM_C8R2M4{ {
        {.cell_size = 16, .offset_x = 616, .offset_y = 300},
        {.cell_size = 20, .offset_x = 420, .offset_y = 300},
        {.cell_size = 24, .offset_x = 548, .offset_y = 76},
        {.cell_size = 28, .offset_x = 484, .offset_y = 232},
        {.cell_size = 32, .offset_x = 548, .offset_y = 0},
        {.cell_size = 40, .offset_x = 484, .offset_y = 140},
        {.cell_size = 48, .offset_x = 0,   .offset_y = 264},
        {.cell_size = 56, .offset_x = 0,   .offset_y = 140},
        {.cell_size = 64, .offset_x = 0,   .offset_y = 0},
    } };

    constexpr std::array<SubSheetMeta, 9> SSM_C8R7M4{ {
        {.cell_size = 16, .offset_x = 616, .offset_y = 1020},
        {.cell_size = 20, .offset_x = 420, .offset_y = 1020},
        {.cell_size = 24, .offset_x = 548, .offset_y = 256},
        {.cell_size = 28, .offset_x = 484, .offset_y = 792},
        {.cell_size = 32, .offset_x = 548, .offset_y = 0},
        {.cell_size = 40, .offset_x = 484, .offset_y = 480},
        {.cell_size = 48, .offset_x = 0,   .offset_y = 904},
        {.cell_size = 56, .offset_x = 0,   .offset_y = 480},
        {.cell_size = 64, .offset_x = 0,   .offset_y = 0},
    } };

    constexpr std::array<SubSheetMeta, 9> SSM_C8R8M4{ {
        {.cell_size = 16, .offset_x = 1164, .offset_y = 616},
        {.cell_size = 20, .offset_x = 1164, .offset_y = 420},
        {.cell_size = 24, .offset_x = 292,  .offset_y = 548},
        {.cell_size = 28, .offset_x = 904,  .offset_y = 484},
        {.cell_size = 32, .offset_x = 0,    .offset_y = 548},
        {.cell_size = 40, .offset_x = 548,  .offset_y = 484},
        {.cell_size = 48, .offset_x = 1032, .offset_y = 0},
        {.cell_size = 56, .offset_x = 548,  .offset_y = 0},
        {.cell_size = 64, .offset_x = 0,    .offset_y = 0},
    } };

    // weather icons: wcc
    {
        const auto icons = std::make_shared<CompositedIconSheet>();
        icons->Load(IDB_PNG_MS_WCC, 8, 7, 4,
                    SubShteetMetaList{ SSM_C8R7M4.begin(), SSM_C8R7M4.end() });
        icon_sheets_[IconResType::WccBlue] = icons;
        icon_sheets_[IconResType::WccWhite] = icons;
    }

    // weather icons: qweather fill & hollow
    {
        const auto icons_fill = std::make_shared<CompositedIconSheet>();
        icons_fill->Load(IDB_PNG_MS_QW_FILL, 8, 8, 4,
                         SubShteetMetaList{ SSM_C8R8M4.begin(), SSM_C8R8M4.end() });
        icon_sheets_[IconResType::QWeatherFill] = icons_fill;

        const auto icons_hollow = std::make_shared<CompositedIconSheet>();
        icons_hollow->Load(IDB_PNG_MS_QW_HOLLOW, 8, 8, 4,
                           SubShteetMetaList{ SSM_C8R8M4.begin(), SSM_C8R8M4.end() });
        icon_sheets_[IconResType::QWeatherHollow] = icons_hollow;
    }

    // weather icons: open weather
    {
        const auto icons = std::make_shared<CompositedIconSheet>();
        icons->Load(IDB_PNG_MS_OW, 8, 2, 4,
                    SubShteetMetaList{ SSM_C8R2M4.begin(), SSM_C8R2M4.end() });
        icon_sheets_[IconResType::OpenWeather] = icons;
    }

    // loading frame icons
    {
        const auto icons_loading = std::make_shared<CompositedIconSheet>();
        icons_loading->Load(IDB_PNG_MS_LOADING, 8, 2, 4,
                            SubShteetMetaList{ SSM_C8R2M4.begin(), SSM_C8R2M4.end() });
        icon_sheets_[IconResType::Loading] = icons_loading;
    }

    // shutdown gdi+
    Gdiplus::GdiplusShutdown(token);
}
