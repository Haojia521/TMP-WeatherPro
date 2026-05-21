#pragma once
#include "afxdialogex.h"


// OptionsOwDlg 对话框

class OptionsOwDlg : public CDialogEx
{
	DECLARE_DYNAMIC(OptionsOwDlg)

public:
	OptionsOwDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~OptionsOwDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_OPTIONS_OW };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	CString str_api_key;
	CComboBox ctrl_combobox_unit_type;
	BOOL bool_use_onecall;
public:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
};
