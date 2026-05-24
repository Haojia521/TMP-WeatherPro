#pragma once
#include "afxdialogex.h"
#include "DataManager.h"

#include <WPCore/DataDef.h>

// MainSettingsDlg 对话框

class MainSettingsDlg : public CDialogEx
{
	DECLARE_DYNAMIC(MainSettingsDlg)

public:
	MainSettingsDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~MainSettingsDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_SETTINGS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	CComboBox ctrl_combobox_data_source;
	CComboBox ctrl_combobox_weather_type;
	CComboBox ctrl_combobox_update_interval;
	BOOL bool_auto_locating;
	BOOL bool_show_info_in_tooltip;
	BOOL bool_draw_icon;
	BOOL bool_draw_alerts_notification_dot;
	BOOL bool_main_item_scroll_text;
	int int_ldc_action;

	Location current_location;
	CString str_current_location;

	DataManager::Config GetConfigsFromUI();

public:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnBnClickedButtonSetLocation();
	afx_msg void OnBnClickedButtonOpenApiSettings();
	afx_msg void OnBnClickedButtonCheckErrorMsg();
	afx_msg void OnBnClickedButtonCheckAlertMsg();
	afx_msg void OnBnClickedButtonUpdateManually();
	afx_msg void OnBnClickedButtonAutoLocOptions();
	afx_msg void OnBnClickedButtonDonate();
	afx_msg void OnBnClickedButtonSetPinnedItems();
	afx_msg void OnBnClickedButtonVersion();
};
