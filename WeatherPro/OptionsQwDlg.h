#pragma once
#include "afxdialogex.h"

// OptionsQwDlg 对话框

class OptionsQwDlg : public CDialogEx
{
	DECLARE_DYNAMIC(OptionsQwDlg)

public:
	OptionsQwDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~OptionsQwDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_OPTIONS_QW };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	CString str_api_host;
	CString str_app_key;
	int int_auth_mode;
	CString str_jwt_proj_id;
	CString str_jwt_cred_id;
	CString str_jwt_pub_key_filepath;
	CString str_jwt_prv_key_filepath;
	BOOL bool_show_temp_feels_like;
	BOOL bool_rt_show_hum;
	BOOL bool_rt_show_wind;
	BOOL bool_rt_use_wind_scale;
	BOOL bool_rt_show_pm2p5;
	BOOL bool_rt_show_pm10;
	BOOL bool_fc_show_hum;
	BOOL bool_fc_show_uvi;
	CComboBox ctrl_combobox_icon_style;

	void SetControlAvailabilityByAuthMode();
public:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnBnClickedRadioAuth();
	afx_msg void OnBnClickedButtonCreateKeyPair();
	afx_msg void OnBnClickedButtonSelectKeyPair();
	afx_msg void OnBnClickedButtonCopyPubKey();
};
