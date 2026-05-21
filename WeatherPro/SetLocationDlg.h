#pragma once
#include "afxdialogex.h"

#include "EditCtrlFloatNumber.h"
#include "WeatherAPI.h"

// SetLocationDlg 对话框

class SetLocationDlg : public CDialogEx
{
	DECLARE_DYNAMIC(SetLocationDlg)

public:
	SetLocationDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~SetLocationDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_SET_LOCTION };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	CEditFloatNumber ctrl_edit_lon;
	CEditFloatNumber ctrl_edit_lat;
	CListCtrl ctrl_list_locations;

	int int_query_type;
	CString str_query_text;
	CString str_query_lon;
	CString str_query_lat;

	Locations locations;

	void EnableControlsByQueryType();
	afx_msg void OnBnClickedRadioQueryType();
	afx_msg void OnBnClickedButtonDoQuery();
public:
	virtual BOOL OnInitDialog();

	ApiType api_type{ ApiType::WeatherComCnSpider };
	Location selected_location;
	virtual void OnOK();
};
