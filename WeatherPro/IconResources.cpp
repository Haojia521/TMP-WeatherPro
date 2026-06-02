#include "pch.h"
#include "resource.h"
#include "IconResources.h"

PngIconSheet::PngIconSheet()
    : hbitmap_(nullptr)
    , cell_width_(0)
    , cell_height_(0)
    , image_width_(0)
    , image_height_(0)
    , cols_(0)
    , rows_(0)
    , margin_(0)
{}

PngIconSheet::~PngIconSheet() {
    Reset();
}

void PngIconSheet::Reset() {
    ::DeleteObject(hbitmap_);
    hbitmap_ = nullptr;
    cell_width_ = 0;
    cell_height_ = 0;
    image_width_ = 0;
    image_height_ = 0;
    cols_ = 0;
    rows_ = 0;
    margin_ = 0;
}

bool PngIconSheet::IsLoaded() const {
    return hbitmap_ != nullptr;
}

int PngIconSheet::GetCellWidth() const {
    return cell_width_;
}

int PngIconSheet::GetCellHeight() const {
    return cell_height_;
}

int PngIconSheet::GetImageWidth() const {
    return image_width_;
}

int PngIconSheet::GetImageHeight() const {
    return image_height_;
}

int PngIconSheet::GetCols() const {
    return cols_;
}

int PngIconSheet::GetRows() const {
    return rows_;
}

int PngIconSheet::GetMaxCount() const {
    return rows_ * cols_;
}

std::unique_ptr<Gdiplus::Bitmap> PngIconSheet::LoadPngFromResource(UINT resourceId) {
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

bool PngIconSheet::Load(UINT resourceId, int cellWidth, int cellHeight, int margin /*= 0*/) {
    Reset();

    if (cellWidth <= 0 || cellHeight <= 0)
        return false;

    margin = std::max(margin, 0);

    std::unique_ptr<Gdiplus::Bitmap> bmp = LoadPngFromResource(resourceId);
    if (!bmp)
        return false;

    const UINT width = bmp->GetWidth();
    const UINT height = bmp->GetHeight();

    if (width == 0 || height == 0)
        return false;

    if (width <= static_cast<UINT>(margin) || height <= static_cast<UINT>(margin)) {
        return false;
    }

    if (((width - static_cast<UINT>(margin)) % static_cast<UINT>(cellWidth + margin)) != 0)
        return false;

    if (((height - static_cast<UINT>(margin)) % static_cast<UINT>(cellHeight + margin)) != 0)
        return false;

    if (hbitmap_) {
        ::DeleteObject(hbitmap_);
        hbitmap_ = nullptr;
    }

    if (bmp->GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbitmap_) != Gdiplus::Ok) {
        hbitmap_ = nullptr;
        return false;
    }
    
    cell_width_ = cellWidth;
    cell_height_ = cellHeight;
    image_width_ = static_cast<int>(width);
    image_height_ = static_cast<int>(height);
    cols_ = (image_width_ - margin) / (cell_width_ + margin);
    rows_ = (image_height_ - margin) / (cell_height_ + margin);
    margin_ = margin;

    return true;
}

bool PngIconSheet::Draw(CDC* pDC, const CRect& destRect, int row, int col) const {
    if (pDC == nullptr)
        return false;

    if (!IsLoaded())
        return false;

    if (destRect.Width() <= 0 || destRect.Height() <= 0)
        return false;

    if (row < 0 || row >= rows_ || col < 0 || col >= cols_)
        return false;

    const int srcX = margin_ + col * (cell_width_ + margin_);
    const int srcY = margin_ + row * (cell_height_ + margin_);

    HDC hdc = ::CreateCompatibleDC(pDC->GetSafeHdc());
    if (!hdc)
        return false;

    HGDIOBJ oldObj = ::SelectObject(hdc, hbitmap_);

    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    BOOL ok = ::AlphaBlend(
        pDC->GetSafeHdc(),
        destRect.left,
        destRect.top,
        destRect.Width(),
        destRect.Height(),
        hdc,
        srcX,
        srcY,
        cell_width_,
        cell_height_,
        blend
    );

    ::SelectObject(hdc, oldObj);
    ::DeleteDC(hdc);

    return ok != FALSE;
}

bool PngIconSheet::Draw(CDC* pDC, const CRect& destRect, int index) const {
    if (!IsLoaded())
        return false;

    if (index < 0 || index >= GetMaxCount())
        return FALSE;

    const int row = index / cols_;
    const int col = index % cols_;

    return Draw(pDC, destRect, row, col);
}

/**********************************************************************************************************************/
// IconSheetManager

IconSheetManager& IconSheetManager::Instance() {
    static IconSheetManager instance;

    return instance;
}

const IconSheet* IconSheetManager::GetIconSheet(IconResType type) {
    if (icon_resources_.contains(type)) {
        return icon_resources_[type].get();
    } else {
        return nullptr;
    }
}

void IconSheetManager::LoadIconResources() {
    AFX_MANAGE_STATE(AfxGetStaticModuleState())

    // start gdi+ temporarily
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;

    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok)
        return;

    // weather icons: wcc
    {
        auto icon_res = std::make_shared<IconSheet>();
        icon_res->Load(IDB_PNG_ICONS_WCC, 32, 32);

        icon_resources_[IconResType::WccBlue] = icon_res;
        icon_resources_[IconResType::WccWhite] = icon_res;
    }

    // weather icons: qweather fill & hollow
    {
        auto icons_res_fill = std::make_shared<IconSheet>();
        icons_res_fill->Load(IDB_PNG_QW_FILL, 32, 32, 4);
        icon_resources_[IconResType::QWeatherFill] = icons_res_fill;

        auto icons_res_hollow = std::make_shared<IconSheet>();
        icons_res_hollow->Load(IDB_PNG_QW_HOLLOW, 32, 32, 4);
        icon_resources_[IconResType::QWeatherHollow] = icons_res_hollow;
    }

    // weather icons: open weather
    {
        auto icons_res = std::make_shared<IconSheet>();
        icons_res->Load(IDB_PNG_OW, 32, 32, 4);
        icon_resources_[IconResType::OpenWeather] = icons_res;
    }

    // loading frame icons
    {
        auto icons_loading = std::make_shared<IconSheet>();
        icons_loading->Load(IDB_PNG_LOADING_SIZE32, 32, 32, 4);
        icon_resources_[IconResType::Loading] = icons_loading;
    }

    // shutdown gdi+
    Gdiplus::GdiplusShutdown(token);
}
