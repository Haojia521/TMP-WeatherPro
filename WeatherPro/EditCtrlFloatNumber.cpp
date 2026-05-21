// FloatEdit.cpp
#include "pch.h"
#include "EditCtrlFloatNumber.h"
#include <afxwin.h>
#include <cctype>

BEGIN_MESSAGE_MAP(CEditFloatNumber, CEdit)
    ON_WM_CHAR()
END_MESSAGE_MAP()

// 验证字符串是否为合法浮点数
BOOL CEditFloatNumber::IsValidFloatString(const CString& str, BOOL strict)
{
    if (str.IsEmpty())
        return TRUE;  // 允许空字符串

    // 只检查ASCII字符
    for (int i = 0; i < str.GetLength(); ++i) {
        if (str[i] < 0 || str[i] > 128) {
            return FALSE;
        }
    }

    // 检查首字符：只能是数字、负号或小数点
    if (!isdigit(str[0]) && str[0] != '-' && str[0] != '.')
        return FALSE;

    // 全局检查
    BOOL hasDecimal = FALSE;  // 小数点标记
    BOOL hasDigit = FALSE;    // 数字标记

    for (int i = 0; i < str.GetLength(); ++i)
    {
        TCHAR c = str[i];

        // 允许数字
        if (isdigit(c)) {
            hasDigit = TRUE;
            continue;
        }

        // 处理负号：只能在开头
        if (c == '-') {
            if (i != 0) return FALSE;  // 负号不在开头
            continue;
        }

        // 处理小数点
        if (c == '.') {
            if (hasDecimal) return FALSE;  // 已有小数点
            hasDecimal = TRUE;
            continue;
        }

        return FALSE;  // 非法字符
    }

    if (strict) {
        return hasDigit;  // 至少需要一位数字
    } else {
        return TRUE;
    }
}

// 处理键盘输入
void CEditFloatNumber::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    // 允许控制字符（退格、删除等）
    if (nChar == VK_BACK || nChar == VK_ESCAPE) {
        CEdit::OnChar(nChar, nRepCnt, nFlags);
        return;
    }

    // 获取当前文本和选择范围
    CString strText;
    GetWindowText(strText);
    int nStart, nEnd;
    GetSel(nStart, nEnd);

    // 构建新字符串（模拟输入后效果）
    CString strNew = strText;
    if (nStart != nEnd) // 替换选中文本
        strNew.Delete(nStart, nEnd - nStart);
    strNew.Insert(nStart, (TCHAR)nChar);

    // 验证新字符串
    if (IsValidFloatString(strNew)) {
        CEdit::OnChar(nChar, nRepCnt, nFlags);
    } else {
        // 非法输入：播放警告声
        MessageBeep(MB_ICONWARNING);
    }
}
