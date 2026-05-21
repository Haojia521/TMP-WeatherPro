// OptionsOwDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "afxdialogex.h"
#include "OptionsOwDlg.h"
#include "resource.h"

#include "Common.h"
#include "DataManager.h"

// OptionsOwDlg 对话框

IMPLEMENT_DYNAMIC(OptionsOwDlg, CDialogEx)

OptionsOwDlg::OptionsOwDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_OPTIONS_OW, pParent)
	, str_api_key(_T(""))
	, bool_use_onecall(FALSE)
{

}

OptionsOwDlg::~OptionsOwDlg()
{
}

void OptionsOwDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_API_KEY, str_api_key);
	DDX_Control(pDX, IDC_COMBO_UNIT_TYPE, ctrl_combobox_unit_type);
	DDX_Check(pDX, IDC_CHECK_USE_ONECALL, bool_use_onecall);
}


BEGIN_MESSAGE_MAP(OptionsOwDlg, CDialogEx)
END_MESSAGE_MAP()


// OptionsOwDlg 消息处理程序

BOOL OptionsOwDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	const auto &cfg = DataManager::Instance().GetApiCollections().GetApiOpenWeather()
		.GetProviderOpenWeather().config;

	str_api_key = cmn::MultiByte2WideChar(cfg.api_key).c_str();

	ctrl_combobox_unit_type.AddString(cmn::GetStringRes(IDS_UNITS_STANDARD));
	ctrl_combobox_unit_type.AddString(cmn::GetStringRes(IDS_UNITS_METRIC));
	ctrl_combobox_unit_type.AddString(cmn::GetStringRes(IDS_UNITS_IMPERIAL));
	ctrl_combobox_unit_type.SetCurSel(static_cast<int>(cfg.unit_type));

	bool_use_onecall = cfg.use_onecall ? TRUE : FALSE;

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void OptionsOwDlg::OnOK()
{
	UpdateData(TRUE);

	DataProviderOpenWeather::Config cfg;
	cfg.api_key = cmn::WideChar2MultiByte(str_api_key);
	cfg.unit_type = static_cast<DataProviderOpenWeather::UnitType>(ctrl_combobox_unit_type.GetCurSel());
	cfg.use_onecall = bool_use_onecall == TRUE;

	auto &provider = DataManager::Instance().GetApiCollections().GetApiOpenWeather().GetProviderOpenWeather();
	provider.config = cfg;

	CDialogEx::OnOK();
}
