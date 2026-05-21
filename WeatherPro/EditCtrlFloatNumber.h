// FloatEdit.h
#pragma once
#include <afxwin.h>

class CEditFloatNumber : public CEdit
{
    DECLARE_MESSAGE_MAP()

    // 处理字符输入
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
public:
    static BOOL IsValidFloatString(const CString& str, BOOL strict = FALSE);
};