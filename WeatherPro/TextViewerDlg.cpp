// TextViewerDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "afxdialogex.h"
#include "TextViewerDlg.h"
#include "resource.h"


// TextViewerDlg 对话框

IMPLEMENT_DYNAMIC(TextViewerDlg, CDialogEx)

TextViewerDlg::TextViewerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_TEXT_VIEWER, pParent)
	, str_text_content(_T(""))
{

}

TextViewerDlg::~TextViewerDlg()
{
}

void TextViewerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_TEXT, str_text_content);
	DDX_Control(pDX, IDC_EDIT_TEXT, ctrl_edit_text_content);
}


BEGIN_MESSAGE_MAP(TextViewerDlg, CDialogEx)
	ON_WM_GETMINMAXINFO()
	ON_MESSAGE(WM_SET_EDIT_CURSOR_BEGIN, &TextViewerDlg::OnSetEditCursorBegin)
    ON_MESSAGE(WM_SET_EDIT_CURSOR_END, &TextViewerDlg::OnSetEditCursorEnd)
END_MESSAGE_MAP()


// TextViewerDlg 消息处理程序

BOOL TextViewerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	if (!str_title.IsEmpty()) {
		SetWindowText(str_title);
	}

	CRect rect;
	GetWindowRect(&rect);
	min_window_size = rect.Size();

	if (set_edit_cursor_at_end) {
		PostMessage(WM_SET_EDIT_CURSOR_END);
	}else {
	    PostMessage(WM_SET_EDIT_CURSOR_BEGIN);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void TextViewerDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	if (min_window_size.cx > 0 && min_window_size.cy > 0)
	{
		lpMMI->ptMinTrackSize.x = min_window_size.cx;
		lpMMI->ptMinTrackSize.y = min_window_size.cy;
	}

	CDialogEx::OnGetMinMaxInfo(lpMMI);
}

LRESULT TextViewerDlg::OnSetEditCursorBegin(WPARAM, LPARAM)
{
	ctrl_edit_text_content.SetFocus();
	ctrl_edit_text_content.SetSel(0, 0);

	return 0;
}

LRESULT TextViewerDlg::OnSetEditCursorEnd(WPARAM, LPARAM) {
	ctrl_edit_text_content.SetFocus();
	ctrl_edit_text_content.SetSel(-1, -1);
	ctrl_edit_text_content.LineScroll(ctrl_edit_text_content.GetLineCount());

	return 0;
}
