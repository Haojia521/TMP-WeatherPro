#pragma once
#include "afxdialogex.h"


// AutoLocSettingsDlg 对话框

class AutoLocSettingsDlg : public CDialogEx
{
	DECLARE_DYNAMIC(AutoLocSettingsDlg)

public:
	AutoLocSettingsDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~AutoLocSettingsDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_SETTINGS_AUTO_LOC };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	BOOL bool_loc_src_api;
	BOOL bool_loc_src_os;
	BOOL bool_loc_src_ip_geo_coord;
	BOOL bool_loc_src_ip_region_name;
public:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
};
