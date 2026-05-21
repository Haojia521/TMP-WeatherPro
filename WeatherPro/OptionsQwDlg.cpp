// OptionsQwDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "OptionsQwDlg.h"
#include "resource.h"

#include "Common.h"
#include "DataManager.h"

#include <WPCore/utils.h>

#include <filesystem>
#include <format>
#include <fstream>

namespace
{
	CString SelectFolder(HWND owner)
	{
		CString folder_path = _T("");

		CComPtr<IFileDialog> ptr_file_dialog;
		HRESULT hr = ptr_file_dialog.CoCreateInstance(CLSID_FileOpenDialog);

		if (SUCCEEDED(hr))
		{
			// 设置选项 - 只选择文件夹
			DWORD dw_options;
			ptr_file_dialog->GetOptions(&dw_options);
			ptr_file_dialog->SetOptions(dw_options | FOS_PICKFOLDERS);

			// 显示对话框
			hr = ptr_file_dialog->Show(owner);

			if (SUCCEEDED(hr))
			{
				CComPtr<IShellItem> ptr_item;
				hr = ptr_file_dialog->GetResult(&ptr_item);

				if (SUCCEEDED(hr))
				{
					PWSTR pszFilePath;
					hr = ptr_item->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

					if (SUCCEEDED(hr))
					{
						folder_path = CString(pszFilePath);
						CoTaskMemFree(pszFilePath);
					}
				}
			}
		}

		return folder_path;
	}

	bool CopyCStringToClipboard(const CString& text, HWND hWnd)
	{
		const SIZE_T bytes = static_cast<SIZE_T>(text.GetLength() + 1) * sizeof(wchar_t);

		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (!hMem) {
			return false;
		}

		void* pMem = GlobalLock(hMem);
		if (!pMem) {
			GlobalFree(hMem);
			return false;
		}

		memcpy(pMem, text.GetString(), bytes);
		GlobalUnlock(hMem);

		if (!OpenClipboard(hWnd)) {
			GlobalFree(hMem);
			return false;
		}

		EmptyClipboard();

		if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
			CloseClipboard();
			GlobalFree(hMem);
			return false;
		}

		CloseClipboard();
		return true;
	}
}

// OptionsQwDlg 对话框

IMPLEMENT_DYNAMIC(OptionsQwDlg, CDialogEx)

OptionsQwDlg::OptionsQwDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_OPTIONS_QW, pParent)
	, str_api_host(_T(""))
	, str_app_key(_T(""))
	, int_auth_mode(0)
	, str_jwt_proj_id(_T(""))
	, str_jwt_cred_id(_T(""))
	, str_jwt_pub_key_filepath(_T(""))
	, str_jwt_prv_key_filepath(_T(""))
	, bool_show_temp_feels_like(FALSE)
	, bool_rt_show_hum(FALSE)
	, bool_rt_show_wind(FALSE)
	, bool_rt_use_wind_scale(FALSE)
	, bool_rt_show_pm2p5(FALSE)
	, bool_rt_show_pm10(FALSE)
	, bool_fc_show_hum(FALSE)
	, bool_fc_show_uvi(FALSE)
{

}

OptionsQwDlg::~OptionsQwDlg()
{
}

void OptionsQwDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_API_HOST, str_api_host);
	DDX_Text(pDX, IDC_EDIT_APP_KEY, str_app_key);
	DDX_Radio(pDX, IDC_RADIO_AUTH_BY_KEY, int_auth_mode);
	DDX_Text(pDX, IDC_EDIT_JWT_PROJ_ID, str_jwt_proj_id);
	DDX_Text(pDX, IDC_EDIT_JWT_CRED_ID, str_jwt_cred_id);
	DDX_Text(pDX, IDC_STATIC_JWT_PUB_KEY_FILEPATH, str_jwt_pub_key_filepath);
	DDX_Text(pDX, IDC_STATIC_JWT_PRV_KEY_FILEPATH, str_jwt_prv_key_filepath);
	DDX_Check(pDX, IDC_CHECK_SHOW_TEMP_FEELS_LIKE, bool_show_temp_feels_like);
	DDX_Check(pDX, IDC_CHECK_SHOW_HUM, bool_rt_show_hum);
	DDX_Check(pDX, IDC_CHECK_SHOW_WIND, bool_rt_show_wind);
	DDX_Check(pDX, IDC_CHECK_USE_WIND_SCALE, bool_rt_use_wind_scale);
	DDX_Check(pDX, IDC_CHECK_SHOW_PM2P5, bool_rt_show_pm2p5);
	DDX_Check(pDX, IDC_CHECK_SHOW_PM10, bool_rt_show_pm10);
	DDX_Check(pDX, IDC_CHECK_FC_SHOW_HUM, bool_fc_show_hum);
	DDX_Check(pDX, IDC_CHECK_FC_SHOW_UVI, bool_fc_show_uvi);
	DDX_Control(pDX, IDC_COMBO_ICON_STYLE, ctrl_combobox_icon_style);
}


BEGIN_MESSAGE_MAP(OptionsQwDlg, CDialogEx)
	ON_BN_CLICKED(IDC_RADIO_AUTH_BY_KEY, &OptionsQwDlg::OnBnClickedRadioAuth)
	ON_BN_CLICKED(IDC_RADIO_AUTH_BY_JWT, &OptionsQwDlg::OnBnClickedRadioAuth)
	ON_BN_CLICKED(IDC_BUTTON_CREATE_KEY_PAIR, &OptionsQwDlg::OnBnClickedButtonCreateKeyPair)
	ON_BN_CLICKED(IDC_BUTTON_SELECT_KEY_PAIR, &OptionsQwDlg::OnBnClickedButtonSelectKeyPair)
	ON_BN_CLICKED(IDC_BUTTON_COPY_PUB_KEY, &OptionsQwDlg::OnBnClickedButtonCopyPubKey)
END_MESSAGE_MAP()

void OptionsQwDlg::SetControlAvailabilityByAuthMode() {
	const bool jwt_enabled = int_auth_mode == 1;

	GetDlgItem(IDC_EDIT_APP_KEY)->EnableWindow(!jwt_enabled);

	GetDlgItem(IDC_EDIT_JWT_PROJ_ID)->EnableWindow(jwt_enabled);
	GetDlgItem(IDC_EDIT_JWT_CRED_ID)->EnableWindow(jwt_enabled);
	GetDlgItem(IDC_STATIC_JWT_PUB_KEY_FILEPATH)->EnableWindow(jwt_enabled);
	GetDlgItem(IDC_STATIC_JWT_PRV_KEY_FILEPATH)->EnableWindow(jwt_enabled);
	GetDlgItem(IDC_BUTTON_CREATE_KEY_PAIR)->EnableWindow(jwt_enabled);
	GetDlgItem(IDC_BUTTON_SELECT_KEY_PAIR)->EnableWindow(jwt_enabled);
	GetDlgItem(IDC_BUTTON_COPY_PUB_KEY)->EnableWindow(jwt_enabled);
}

// OptionsQwDlg 消息处理程序

BOOL OptionsQwDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	const auto &api = DataManager::Instance().GetApiCollections().GetApiQWeather();
	const auto &cfg_gui = api.config;
	const auto &cfg_app = api.GetProviderQWeather().config_app;
	const auto &cfg_fmt = api.GetProviderQWeather().config_fmt;

	ctrl_combobox_icon_style.AddString(cmn::GetStringRes(IDS_QW_ICON_FILL));
	ctrl_combobox_icon_style.AddString(cmn::GetStringRes(IDS_QW_ICON_HOLLOW));
	ctrl_combobox_icon_style.SetCurSel(static_cast<int>(cfg_gui.icon_style));

	str_api_host = cmn::MultiByte2WideChar(cfg_app.api_host).c_str();
	str_app_key = cmn::MultiByte2WideChar(cfg_app.app_key).c_str();
	
	str_jwt_proj_id = cmn::MultiByte2WideChar(cfg_app.project_id).c_str();
	str_jwt_cred_id = cmn::MultiByte2WideChar(cfg_app.credential_id).c_str();
	str_jwt_pub_key_filepath = cmn::MultiByte2WideChar(cfg_app.jwt_pub_key_file).c_str();
	str_jwt_prv_key_filepath = cmn::MultiByte2WideChar(cfg_app.jwt_prv_key_file).c_str();

	int_auth_mode = cfg_app.enable_jwt ? 1 : 0;
	SetControlAvailabilityByAuthMode();

	bool_show_temp_feels_like = cfg_fmt.show_realtime_temp_feels_like;
	bool_rt_show_hum = cfg_fmt.show_realtime_humidity;
	bool_rt_show_wind = cfg_fmt.show_realtime_wind;
	bool_rt_use_wind_scale = cfg_fmt.show_realtime_wind_scale;
	bool_rt_show_pm2p5 = cfg_fmt.show_realtime_pm2p5;
	bool_rt_show_pm10 = cfg_fmt.show_realtime_pm10;
	bool_fc_show_hum = cfg_fmt.show_forecasted_humidity;
	bool_fc_show_uvi = cfg_fmt.show_forecasted_uv_index;

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void OptionsQwDlg::OnOK()
{
	UpdateData(TRUE);

	auto &api = DataManager::Instance().GetApiCollections().GetApiQWeather();
	//auto &cfg_gui = api.config;
	//auto &cfg_app = api.GetProviderQWeather().config_app;
	//auto &cfg_fmt = api.GetProviderQWeather().config_fmt;

	WapiQWeather::Config cfg_gui{
		.icon_style = static_cast<WapiQWeather::IconStyle>(ctrl_combobox_icon_style.GetCurSel()),
	};

	DataProviderQWeather::ConfigApp cfg_app{
		.app_key = cmn::WideChar2MultiByte(str_app_key),
		.api_host = cmn::WideChar2MultiByte(str_api_host),
		.project_id = cmn::WideChar2MultiByte(str_jwt_proj_id),
		.credential_id = cmn::WideChar2MultiByte(str_jwt_cred_id),
		.jwt_pub_key_file = cmn::WideChar2MultiByte(str_jwt_pub_key_filepath),
		.jwt_prv_key_file = cmn::WideChar2MultiByte(str_jwt_prv_key_filepath),
		.enable_jwt = int_auth_mode == 1,
	};

	DataProviderQWeather::ConfigFormatting cfg_fmt{
		.show_realtime_temp_feels_like = bool_show_temp_feels_like == TRUE,
		.show_realtime_wind = bool_rt_show_wind == TRUE,
		.show_realtime_wind_scale = bool_rt_use_wind_scale == TRUE,
		.show_realtime_humidity = bool_rt_show_hum == TRUE,
		.show_realtime_pm2p5 = bool_rt_show_pm2p5 == TRUE,
		.show_realtime_pm10 = bool_rt_show_pm10 == TRUE,
		.show_forecasted_uv_index = bool_fc_show_uvi == TRUE,
		.show_forecasted_humidity = bool_fc_show_hum == TRUE,
	};

	auto changed_auth_config = cfg_app != api.GetProviderQWeather().config_app;
	auto changed_fmt_config =  cfg_fmt != api.GetProviderQWeather().config_fmt;

	api.config = cfg_gui;
	api.GetProviderQWeather().config_app = cfg_app;
	api.GetProviderQWeather().config_fmt = cfg_fmt;
	DataManager::Instance().SaveConfigs();

	if (DataManager::Instance().GetConfig().api_type == ApiType::QWeather) {
		if (changed_auth_config) {
			WeatherPro::Instance().UpdateWeather(true);
		} else if (changed_fmt_config) {
			DataManager::Instance().RefreshCache();
		}
	}

	CDialogEx::OnOK();
}

void OptionsQwDlg::OnBnClickedRadioAuth()
{
	UpdateData(TRUE);
	SetControlAvailabilityByAuthMode();
}

void OptionsQwDlg::OnBnClickedButtonCreateKeyPair()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	auto folder_path = SelectFolder(nullptr);
	if (folder_path.IsEmpty()) {
		return;
	}

	std::wstring pub_key_filepath = std::format(L"{}\\hfw-ed25519-public.pem", folder_path.GetString());
	std::wstring prv_key_filepath = std::format(L"{}\\hfw-ed25519-private.pem", folder_path.GetString());

	auto need_to_generate{ true };

	namespace fs = std::filesystem;
	if (fs::exists(pub_key_filepath) && fs::exists(prv_key_filepath)) {
		auto ret = ::MessageBox(GetSafeHwnd(),
								cmn::GetStringRes(IDS_OPT_QW_SSH_KEYS_EXIST), _T("?"),
								MB_YESNOCANCEL | MB_DEFBUTTON2 | MB_ICONQUESTION);
		if (ret == IDCANCEL) {
			return;
		} else if (ret == IDNO) {
			need_to_generate = false;
		}
	}

	if (need_to_generate) {
		auto [prv_key, pub_key] = utils::generateEd25519Keypair();
		std::ofstream ofs_pri(prv_key_filepath);
		if (ofs_pri.is_open()) {
			ofs_pri << prv_key;
			ofs_pri.close();
		} else {
			MessageBox(cmn::GetStringRes(IDS_OPT_QW_CREATE_KEY_FILE_FAILED), _T("Error"), MB_ICONERROR);
			return;
		}

		std::ofstream ofs_pub(pub_key_filepath);
		if (ofs_pub.is_open()) {
			ofs_pub << pub_key;
			ofs_pub.close();
		} else {
			MessageBox(cmn::GetStringRes(IDS_OPT_QW_CREATE_KEY_FILE_FAILED), _T("Error"), MB_ICONERROR);
			return;
		}
	}

	str_jwt_pub_key_filepath = pub_key_filepath.c_str();
	str_jwt_prv_key_filepath = prv_key_filepath.c_str();

	UpdateData(FALSE);
}

void OptionsQwDlg::OnBnClickedButtonSelectKeyPair()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	auto folder_path = SelectFolder(nullptr);
	if (folder_path.IsEmpty()) {
		return;
	}

	std::wstring pub_key_filepath = std::format(L"{}\\hfw-ed25519-public.pem", folder_path.GetString());
	std::wstring prv_key_filepath = std::format(L"{}\\hfw-ed25519-private.pem", folder_path.GetString());

	namespace fs = std::filesystem;
	if (fs::exists(pub_key_filepath) && fs::exists(prv_key_filepath)) {
		str_jwt_pub_key_filepath = pub_key_filepath.c_str();
		str_jwt_prv_key_filepath = prv_key_filepath.c_str();

		UpdateData(FALSE);
	} else {
		MessageBox(cmn::GetStringRes(IDS_OPT_QW_KEY_FILES_NOT_FOUND), _T("Alert"), MB_ICONINFORMATION);
	}
}

void OptionsQwDlg::OnBnClickedButtonCopyPubKey()
{
	namespace fs = std::filesystem;
	if (fs::exists(str_jwt_pub_key_filepath.GetString())) {
		std::wifstream wifs{ str_jwt_pub_key_filepath.GetString() };

		if (wifs.is_open()) {
			std::wstring pub_key{ std::istreambuf_iterator<wchar_t>(wifs), std::istreambuf_iterator<wchar_t>() };
			wifs.close();

			if (CopyCStringToClipboard(pub_key.c_str(), GetSafeHwnd())) {
				MessageBox(cmn::GetStringRes(IDS_OPT_QW_PUB_KEY_COPIED), _T("Info"), MB_ICONINFORMATION);
			} else {
				MessageBox(cmn::GetStringRes(IDS_OPT_QW_COPY_PUB_KEY_FAILED), _T("Warn"), MB_ICONWARNING);
			}

			return;
		}
	}

	MessageBox(cmn::GetStringRes(IDS_OPT_QW_OPEN_PUB_KEY_FAILED), _T("Warn"), MB_ICONWARNING);
}
