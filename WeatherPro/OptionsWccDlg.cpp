// OptionsWccDlg.cpp: 实现文件
//

#include "pch.h"
#include "resource.h"
#include "WeatherPro.h"
#include "afxdialogex.h"
#include "OptionsWccDlg.h"

#include "Common.h"
#include "DataManager.h"

// OptionsWccDlg 对话框

IMPLEMENT_DYNAMIC(OptionsWccDlg, CDialogEx)

OptionsWccDlg::OptionsWccDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_OPTIONS_WCC, pParent)
	, bool_use_api_location(FALSE)
	, api_wccs(DataManager::Instance().GetApiCollections().GetApiWCCS())
{
}

OptionsWccDlg::~OptionsWccDlg()
{
}

void OptionsWccDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_ICON_STYLE, ctrl_combobox_icon_style);
	DDX_Check(pDX, IDC_CHECK_USE_API_LOCATION, bool_use_api_location);
}


BEGIN_MESSAGE_MAP(OptionsWccDlg, CDialogEx)
END_MESSAGE_MAP()


// OptionsWccDlg 消息处理程序

BOOL OptionsWccDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	const auto &api = api_wccs.get();
	
	ctrl_combobox_icon_style.AddString(cmn::GetStringRes(IDS_WCC_ICON_BLUE));
	ctrl_combobox_icon_style.AddString(cmn::GetStringRes(IDS_WCC_ICON_WHITE));
	ctrl_combobox_icon_style.SetCurSel(static_cast<int>(api.config.icon_type));

	bool_use_api_location = api.GetProviderWCCS().config.use_provider_location;

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void OptionsWccDlg::OnOK()
{
	UpdateData(TRUE);

	auto &api = api_wccs.get();

	api.config.icon_type = static_cast<WapiWCCS::IconStyle>(ctrl_combobox_icon_style.GetCurSel());
	api.GetProviderWCCS().config.use_provider_location = bool_use_api_location == TRUE;

	DataManager::Instance().SaveConfigs();

	CDialogEx::OnOK();
}
