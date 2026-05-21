#pragma once

#include "Types.h"

#include <memory>
#include <unordered_map>

#include <afxwin.h>
#include <gdiplus.h>

class PngIconSheet
{
public:
    PngIconSheet();
    virtual ~PngIconSheet();

    bool Load(UINT resourceId, int cellWidth, int cellHeight, int margin = 0);
    void Reset();

    bool IsLoaded() const;

    int GetCellWidth() const;
    int GetCellHeight() const;
    int GetImageWidth() const;
    int GetImageHeight() const;
    int GetCols() const;
    int GetRows() const;
    int GetMaxCount() const;

    bool Draw(CDC* pDC, const CRect& destRect, int row, int col) const;
    bool Draw(CDC* pDC, const CRect& destRect, int index) const;

private:
    static std::unique_ptr<Gdiplus::Bitmap> LoadPngFromResource(UINT resourceId);

    HBITMAP hbitmap_;
    int cell_width_;
    int cell_height_;
    int image_width_;
    int image_height_;
    int cols_;
    int rows_;
    int margin_;
};

using IconSheet = PngIconSheet;
using IconSheetPtr = std::shared_ptr<IconSheet>;
using IconSheetCPtr = std::shared_ptr<const IconSheet>;

class IconSheetManager
{
public:
    static IconSheetManager& Instance();

    const IconSheet* GetIconSheet(IconResType type);

    void LoadIconResources();

private:
    std::unordered_map<IconResType, IconSheetPtr> icon_resources_;
};
