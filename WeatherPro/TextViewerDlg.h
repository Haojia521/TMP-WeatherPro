#pragma once
#include "afxdialogex.h"


// TextViewerDlg 对话框
#define WM_SET_EDIT_CURSOR_BEGIN (WM_USER + 100)

class TextViewerDlg : public CDialogEx
{
	DECLARE_DYNAMIC(TextViewerDlg)

public:
	TextViewerDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~TextViewerDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_TEXT_VIEWER };
#endif

protected:
	afx_msg LRESULT OnSetEditCursorBegin(WPARAM, LPARAM);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	CEdit ctrl_edit_text_content;
	CSize min_window_size;
public:
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	virtual BOOL OnInitDialog();
	CString str_title;
	CString str_text_content;
};
