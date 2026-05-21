#pragma once
#include "afxdialogex.h"

#include "WeatherApiWCCS.h"

#include <functional>

// OptionsWccDlg 对话框

class OptionsWccDlg : public CDialogEx
{
	DECLARE_DYNAMIC(OptionsWccDlg)

public:
	OptionsWccDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~OptionsWccDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_OPTIONS_WCC };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	CComboBox ctrl_combobox_icon_style;
	BOOL bool_use_api_location;

	std::reference_wrapper<WapiWCCS> api_wccs;

public:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
};
