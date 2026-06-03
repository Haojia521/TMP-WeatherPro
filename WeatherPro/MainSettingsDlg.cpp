// MainSettingsDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "MainSettingsDlg.h"
#include "SetLocationDlg.h"
#include "OptionsWccDlg.h"
#include "OptionsQwDlg.h"
#include "OptionsOwDlg.h"
#include "TextViewerDlg.h"
#include "AutoLocSettingsDlg.h"
#include "PinnedItemSettingsDlg.h"
#include "resource.h"

#include "Common.h"
#include "DataManager.h"

#include <WPCore/Logger.h>

#include <ranges>
#include <sstream>

// MainSettingsDlg 对话框

IMPLEMENT_DYNAMIC(MainSettingsDlg, CDialogEx)

MainSettingsDlg::MainSettingsDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_SETTINGS, pParent)
	, bool_show_info_in_tooltip(FALSE)
	, bool_draw_icon(FALSE)
	, int_ldc_action(0)
	, str_current_location(_T(""))
	, bool_auto_locating(FALSE)
	, bool_draw_alerts_notification_dot(FALSE)
	, bool_main_item_scroll_text(FALSE)
	, bool_show_geo_coords_in_summary(FALSE)
	, bool_enable_dual_line_mode(FALSE)
{

}

MainSettingsDlg::~MainSettingsDlg()
{
}

void MainSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_DATA_SOURCE, ctrl_combobox_data_source);
	DDX_Control(pDX, IDC_COMBO_WEATHER_TYPE, ctrl_combobox_weather_type);
	DDX_Control(pDX, IDC_COMBO_UPDATE_INTERVAL, ctrl_combobox_update_interval);
	DDX_Check(pDX, IDC_CHECK_SHOW_INFO_IN_TOOLTIP, bool_show_info_in_tooltip);
	DDX_Check(pDX, IDC_CHECK_DRAW_ICON, bool_draw_icon);
	DDX_Radio(pDX, IDC_RADIO_LDC_ACT_OPEN_SETTINGS, int_ldc_action);
	DDX_Text(pDX, IDC_STATIC_CURRENT_LOCATION, str_current_location);
	DDX_Check(pDX, IDC_CHECK_AUTO_LOCATING, bool_auto_locating);
	DDX_Check(pDX, IDC_CHECK_DRAW_ALERTS_NOTIFICATION_DOT, bool_draw_alerts_notification_dot);
	DDX_Check(pDX, IDC_CHECK_MAIN_ITEM_SCROLL_TEXT, bool_main_item_scroll_text);
	DDX_Check(pDX, IDC_CHECK_SHOW_GEO_COORDS_IN_SUMMARY, bool_show_geo_coords_in_summary);
	DDX_Check(pDX, IDC_CHECK_DUAL_LINE_MODE, bool_enable_dual_line_mode);
}


BEGIN_MESSAGE_MAP(MainSettingsDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_SET_LOCATION, &MainSettingsDlg::OnBnClickedButtonSetLocation)
	ON_BN_CLICKED(IDC_BUTTON_OPEN_API_SETTINGS, &MainSettingsDlg::OnBnClickedButtonOpenApiSettings)
	ON_BN_CLICKED(IDC_BUTTON_CHECK_ERROR_MSG, &MainSettingsDlg::OnBnClickedButtonCheckErrorMsg)
	ON_BN_CLICKED(IDC_BUTTON_CHECK_ALERT_MSG, &MainSettingsDlg::OnBnClickedButtonCheckAlertMsg)
	ON_BN_CLICKED(IDC_BUTTON_UPDATE_MANUALLY, &MainSettingsDlg::OnBnClickedButtonUpdateManually)
	ON_BN_CLICKED(IDC_BUTTON_AUTO_LOC_OPTIONS, &MainSettingsDlg::OnBnClickedButtonAutoLocOptions)
	ON_BN_CLICKED(IDC_BUTTON_DONATE, &MainSettingsDlg::OnBnClickedButtonDonate)
	ON_BN_CLICKED(IDC_BUTTON_SET_PINNED_ITEMS, &MainSettingsDlg::OnBnClickedButtonSetPinnedItems)
	ON_BN_CLICKED(IDC_BUTTON_VERSION, &MainSettingsDlg::OnBnClickedButtonVersion)
END_MESSAGE_MAP()


// MainSettingsDlg 消息处理程序

BOOL MainSettingsDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	const auto &cfg = DataManager::Instance().GetConfig();

	// initialize list of data sources
	ctrl_combobox_data_source.AddString(cmn::GetStringRes(IDS_API_WCC));
	ctrl_combobox_data_source.AddString(cmn::GetStringRes(IDS_API_QW));
	ctrl_combobox_data_source.AddString(cmn::GetStringRes(IDS_API_OW));
	ctrl_combobox_data_source.SetCurSel(static_cast<int>(cfg.api_type));

	// initialize list of weather info types
	ctrl_combobox_weather_type.AddString(cmn::GetStringRes(IDS_WEATHER_REALTIME));
	ctrl_combobox_weather_type.AddString(cmn::GetStringRes(IDS_WEATHER_IN_24HS));
	ctrl_combobox_weather_type.AddString(cmn::GetStringRes(IDS_WEATHER_IN_24_48HS));
	ctrl_combobox_weather_type.AddString(cmn::GetStringRes(IDS_WEATHER_IN_48_72HS));
	ctrl_combobox_weather_type.SetCurSel(static_cast<int>(cfg.time_slot));

	// initialize list of update intervals
	ctrl_combobox_update_interval.AddString(cmn::GetStringRes(IDS_UPDATE_INTERVAL_05MIN));
	ctrl_combobox_update_interval.AddString(cmn::GetStringRes(IDS_UPDATE_INTERVAL_15MIN));
	ctrl_combobox_update_interval.AddString(cmn::GetStringRes(IDS_UPDATE_INTERVAL_30MIN));
	ctrl_combobox_update_interval.AddString(cmn::GetStringRes(IDS_UPDATE_INTERVAL_60MIN));
	ctrl_combobox_update_interval.AddString(cmn::GetStringRes(IDS_UPDATE_INTERVAL_120MIN));
	ctrl_combobox_update_interval.SetCurSel(static_cast<int>(cfg.update_interval));

	// initialze check box
	bool_auto_locating = cfg.auto_locating ? TRUE : FALSE;
	bool_show_info_in_tooltip = cfg.show_weather_in_tooltip ? TRUE : FALSE;
	bool_draw_icon = cfg.draw_weather_icon ? TRUE : FALSE;
	bool_draw_alerts_notification_dot = cfg.draw_alerts_notification_dot ? TRUE : FALSE;
	bool_main_item_scroll_text = cfg.main_item_scroll_text ? TRUE : FALSE;
	bool_show_geo_coords_in_summary = cfg.format_geo_coords_in_summary ? TRUE : FALSE;
	bool_enable_dual_line_mode = cfg.enable_dual_line_mode ? TRUE : FALSE;

	// initialze radio button
	int_ldc_action = cfg.double_click_action == DataManager::LDoubleClickAction::OpenSettingWindow ? 0 : 1;

	// initialize location string
	const auto &loc = DataManager::Instance().GetCurrentLocation();
	current_location = loc;
	str_current_location = cmn::MultiByte2WideChar(loc.getFormattedString().c_str()).c_str();

	UpdateData(FALSE);

	// new version
	if (!WeatherPro::Instance().HasNewVersionWpPackage()) {
		GetDlgItem(IDC_BUTTON_VERSION)->ShowWindow(SW_HIDE);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

DataManager::Config MainSettingsDlg::GetConfigsFromUI() {
	UpdateData(TRUE);

	const auto &old_cfg = DataManager::Instance().GetConfig();
	
	DataManager::Config cfg;
	cfg.api_type = static_cast<ApiType>(ctrl_combobox_data_source.GetCurSel());
	cfg.time_slot = static_cast<WeatherTimeSlot>(ctrl_combobox_weather_type.GetCurSel());
	cfg.update_interval = static_cast<DataManager::UpdateInterval>(ctrl_combobox_update_interval.GetCurSel());
	cfg.double_click_action = static_cast<DataManager::LDoubleClickAction>(int_ldc_action);
	cfg.auto_locating = bool_auto_locating == TRUE;
	cfg.show_weather_in_tooltip = bool_show_info_in_tooltip == TRUE;
	cfg.draw_weather_icon = bool_draw_icon == TRUE;
	cfg.draw_alerts_notification_dot = bool_draw_alerts_notification_dot == TRUE;
	cfg.main_item_scroll_text = bool_main_item_scroll_text == TRUE;
	cfg.format_geo_coords_in_summary = bool_show_geo_coords_in_summary == TRUE;
	cfg.enable_dual_line_mode = bool_enable_dual_line_mode == TRUE;
	cfg.auto_locating_src = old_cfg.auto_locating_src;
	cfg.pinned_item_data_keys = old_cfg.pinned_item_data_keys;

	return cfg;
}

void MainSettingsDlg::OnOK()
{
	auto &data_mgr = DataManager::Instance();
	const auto &old_cfg = data_mgr.GetConfig();
	const auto &old_loc = data_mgr.GetCurrentLocation();

	auto cfg{ GetConfigsFromUI() };

	auto api_changed = cfg.api_type != old_cfg.api_type;
	auto auto_locating_changed = cfg.auto_locating != old_cfg.auto_locating;
	auto location_changed = current_location != old_loc;

	if (location_changed) {
		data_mgr.SetCurrentLocation(current_location);
	}

	data_mgr.SetConfig(cfg);
	data_mgr.SaveConfigs();

	if (api_changed || auto_locating_changed || location_changed) {
		WeatherPro::Instance().UpdateWeather(true);
	} else {
		data_mgr.RefreshCache();
	}

	CDialogEx::OnOK();
}

void MainSettingsDlg::OnBnClickedButtonSetLocation()
{
	UpdateData(TRUE);

	SetLocationDlg dlg(this);
	dlg.api_type = static_cast<ApiType>(ctrl_combobox_data_source.GetCurSel());
	if (dlg.DoModal() == IDOK) {
		if (const auto &loc = dlg.selected_location; 
			(!loc.name.empty() && !loc.id.empty()) ||
			(!loc.longitude.empty() && !loc.latitude.empty())) {
			current_location = loc;

			// update location string
			str_current_location = cmn::MultiByte2WideChar(loc.getFormattedString().c_str()).c_str();
			// disable auto locating
			bool_auto_locating = FALSE;

			UpdateData(FALSE);
		}
	}
}

void MainSettingsDlg::OnBnClickedButtonOpenApiSettings()
{
	auto gui_api_type = static_cast<ApiType>(ctrl_combobox_data_source.GetCurSel());

	if (gui_api_type == ApiType::WeatherComCnSpider) {
		OptionsWccDlg dlg(this);
		dlg.DoModal();
	} else if (gui_api_type == ApiType::QWeather) {
		OptionsQwDlg dlg(this);
		dlg.DoModal();
	} else if (gui_api_type == ApiType::OpenWeather) {
		OptionsOwDlg dlg(this);
		dlg.DoModal();
	}
}

void MainSettingsDlg::OnBnClickedButtonCheckErrorMsg()
{
	TextViewerDlg dlg(this);
	dlg.str_title = cmn::GetStringRes(IDS_WND_TITLE_LOGGER_MSG);

	std::wostringstream woss;
	for (const auto &msg : Logger::instance().formattedSnapshot() | std::views::reverse) {
		woss << cmn::MultiByte2WideChar(msg) << "\r\n";
	}

	dlg.str_text_content = woss.str().c_str();
	dlg.DoModal();
}

void MainSettingsDlg::OnBnClickedButtonCheckAlertMsg()
{
	TextViewerDlg dlg(this);
	dlg.str_title = cmn::GetStringRes(IDS_WND_TITLE_ALERT_MSG);

	auto data_snapshot = DataManager::GetSnapshot();
	if (data_snapshot != nullptr) {
		dlg.str_text_content = data_snapshot->GetWeatherItem(WeatherTimeSlot::REALTIME, WeatherItem::ALERTS);
		dlg.str_text_content.Replace(L"\n", L"\r\n");
	}

	dlg.DoModal();
}

void MainSettingsDlg::OnBnClickedButtonUpdateManually()
{
	// time interval in seconds between the two manual updates
	constexpr std::time_t update_time_span{ 10 };
	static std::time_t last_update_stamp{ 0 };

	auto now = std::time(nullptr);
	if (now < last_update_stamp + update_time_span) {
		return;
	}

	// save current configs first to make them taking effect
	auto &data_mgr = DataManager::Instance();
	auto cfg{ GetConfigsFromUI() };

	data_mgr.SetCurrentLocation(current_location);
	data_mgr.SetConfig(cfg);
	data_mgr.SaveConfigs();

	// update the timestamp and weather data
	last_update_stamp = now;
	WeatherPro::Instance().UpdateWeather(true);
}

void MainSettingsDlg::OnBnClickedButtonAutoLocOptions()
{
	AutoLocSettingsDlg dlg(this);
	dlg.DoModal();
}

void MainSettingsDlg::OnBnClickedButtonDonate()
{
	CDialogEx dlg(IDD_DLG_DONATE, this);
	dlg.DoModal();
}

void MainSettingsDlg::OnBnClickedButtonSetPinnedItems()
{
	PinnedItemSettingsDlg dlg(this);
	dlg.DoModal();
}

void MainSettingsDlg::OnBnClickedButtonVersion()
{
	const auto &wp_app = WeatherPro::Instance();

	if (wp_app.HasNewVersionWpPackage()) {
		const auto &latest_pkg = wp_app.GetWpLatestPackage();

		const auto &msg_template = cmn::GetStringRes(IDS_NEW_VERSION_DESCRIPTION);
#ifdef WPX86
		auto target_url{ latest_pkg.url_x86 };
#else
		auto target_url{ latest_pkg.url_x64 };
#endif // WPX86

		CString msg;
		msg.Format(msg_template, latest_pkg.version.to_wstring().c_str(), target_url.c_str());
		if (MessageBoxW(msg, cmn::GetStringRes(IDS_NEW_VERSION_RELEASED), MB_OKCANCEL) == IDOK) {
			ShellExecuteW(NULL, _T("open"), target_url.c_str(), NULL, NULL, SW_SHOW);
		}
	}
}
