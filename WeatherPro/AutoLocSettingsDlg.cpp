// AutoLocSettingsDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "afxdialogex.h"
#include "AutoLocSettingsDlg.h"
#include "resource.h"

#include "DataManager.h"

// AutoLocSettingsDlg 对话框

IMPLEMENT_DYNAMIC(AutoLocSettingsDlg, CDialogEx)

AutoLocSettingsDlg::AutoLocSettingsDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_SETTINGS_AUTO_LOC, pParent)
	, bool_loc_src_api(FALSE)
	, bool_loc_src_os(FALSE)
	, bool_loc_src_ip_geo_coord(FALSE)
	, bool_loc_src_ip_region_name(FALSE)
{

}

AutoLocSettingsDlg::~AutoLocSettingsDlg()
{
}

void AutoLocSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK_LOC_SRC_API, bool_loc_src_api);
	DDX_Check(pDX, IDC_CHECK_LOC_SRC_OS, bool_loc_src_os);
	DDX_Check(pDX, IDC_CHECK_LOC_SRC_IP_GEO, bool_loc_src_ip_geo_coord);
	DDX_Check(pDX, IDC_CHECK_LOC_SRC_IP_RGN_NAME, bool_loc_src_ip_region_name);
}


BEGIN_MESSAGE_MAP(AutoLocSettingsDlg, CDialogEx)
END_MESSAGE_MAP()


// AutoLocSettingsDlg 消息处理程序

BOOL AutoLocSettingsDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	auto loc_source = DataManager::Instance().GetConfig().auto_locating_src;

	bool_loc_src_api = HasFlag(loc_source, LocationSource::ApiProvided);
	bool_loc_src_os = HasFlag(loc_source, LocationSource::OsGeolocation);
	bool_loc_src_ip_geo_coord = HasFlag(loc_source, LocationSource::IpGeolocation);
	bool_loc_src_ip_region_name = HasFlag(loc_source, LocationSource::IpRegionName);

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void AutoLocSettingsDlg::OnOK()
{
	UpdateData(TRUE);

	auto loc_source{ LocationSource::None };

	if (bool_loc_src_api) {
		loc_source |= LocationSource::ApiProvided;
	}
	if (bool_loc_src_os) {
		loc_source |= LocationSource::OsGeolocation;
	}
	if (bool_loc_src_ip_geo_coord) {
		loc_source |= LocationSource::IpGeolocation;
	}
	if (bool_loc_src_ip_region_name) {
		loc_source |= LocationSource::IpRegionName;
	}

	DataManager::Config cfg{ DataManager::Instance().GetConfig() };
	if (loc_source != cfg.auto_locating_src) {
		cfg.auto_locating_src = loc_source;
		DataManager::Instance().SetConfig(cfg);
		DataManager::Instance().SaveConfigs();
	}

	CDialogEx::OnOK();
}
