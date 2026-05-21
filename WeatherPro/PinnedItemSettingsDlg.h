#pragma once
#include "afxdialogex.h"
#include "Types.h"

// PinnedItemSettingsDlg 对话框

class PinnedItemSettingsDlg : public CDialogEx
{
	DECLARE_DYNAMIC(PinnedItemSettingsDlg)

public:
	PinnedItemSettingsDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~PinnedItemSettingsDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_SETTINGS_PINNED_ITEM };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	std::vector<WeatherDataKey> current_keys;

public:
	CComboBox ctrl_combo_time_slot;
	CComboBox ctrl_combo_weather_item;
	CListCtrl ctrl_list_current_keys;
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnBnClickedButtonAddKey();
	afx_msg void OnBnClickedButtonRemoveKey();
	afx_msg void OnBnClickedButtonClearKeys();
};
