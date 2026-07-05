#pragma once

#include "Types.h"

#include <memory>
#include <unordered_map>

#include <afxwin.h>

class IconSheet
{
public:
    virtual ~IconSheet() = default;

    virtual bool Draw(CDC &dc, const CRect& dest_rect, int row, int col) const = 0;
    virtual bool Draw(CDC &dc, const CRect& dest_rect, int index) const = 0;

    [[nodiscard]] virtual int GetCols() const = 0;
    [[nodiscard]] virtual int GetRows() const = 0;
    [[nodiscard]] virtual int GetMaxCount() const = 0;
};

using IconSheetPtr = std::shared_ptr<IconSheet>;
using IconSheetCPtr = std::shared_ptr<const IconSheet>;

class IconManager
{
public:
    static IconManager& Instance();

    const IconSheet* GetIconSheet(IconResType type);
    HICON GetIcon(UINT icon_id);

    void LoadIconResources();

private:
    std::unordered_map<IconResType, IconSheetPtr> icon_sheets_;
    std::unordered_map<UINT, HICON> icons_;
};
