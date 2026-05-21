// SetLocationDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "afxdialogex.h"
#include "SetLocationDlg.h"
#include "resource.h"

#include "Common.h"
#include "DataManager.h"

// SetLocationDlg 对话框

IMPLEMENT_DYNAMIC(SetLocationDlg, CDialogEx)

SetLocationDlg::SetLocationDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_SET_LOCTION, pParent)
	, int_query_type(0)
	, str_query_text(_T(""))
	, str_query_lon(_T(""))
	, str_query_lat(_T(""))
{

}

SetLocationDlg::~SetLocationDlg()
{
}

void SetLocationDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_RADIO_QUERY_BY_TEXT, int_query_type);
	DDX_Control(pDX, IDC_EDIT_QUERY_LON, ctrl_edit_lon);
	DDX_Control(pDX, IDC_EDIT_QUERY_LAT, ctrl_edit_lat);
	DDX_Text(pDX, IDC_EDIT_QUERY_TEXT, str_query_text);
	DDX_Text(pDX, IDC_EDIT_QUERY_LON, str_query_lon);
	DDX_Text(pDX, IDC_EDIT_QUERY_LAT, str_query_lat);
	DDX_Control(pDX, IDC_LIST_LOCATIONS, ctrl_list_locations);
}


BEGIN_MESSAGE_MAP(SetLocationDlg, CDialogEx)
	ON_BN_CLICKED(IDC_RADIO_QUERY_BY_TEXT, &SetLocationDlg::OnBnClickedRadioQueryType)
	ON_BN_CLICKED(IDC_RADIO_QUERY_BY_COORD, &SetLocationDlg::OnBnClickedRadioQueryType)
	ON_BN_CLICKED(IDC_BUTTON_DO_QUERY, &SetLocationDlg::OnBnClickedButtonDoQuery)
END_MESSAGE_MAP()

void SetLocationDlg::EnableControlsByQueryType() {
	if (int_query_type == 0) {
		// query location by text
		GetDlgItem(IDC_EDIT_QUERY_TEXT)->EnableWindow(TRUE);

		GetDlgItem(IDC_EDIT_QUERY_LON)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_QUERY_LAT)->EnableWindow(FALSE);
	} else {
		// query location by text
		GetDlgItem(IDC_EDIT_QUERY_TEXT)->EnableWindow(FALSE);

		GetDlgItem(IDC_EDIT_QUERY_LON)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT_QUERY_LAT)->EnableWindow(TRUE);
	}
}

// SetLocationDlg 消息处理程序

BOOL SetLocationDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	int_query_type = 0;
	EnableControlsByQueryType();

	if (api_type == ApiType::WeatherComCnSpider) {
		// ApiType::WeatherComCnSpider only support query locations by text
		GetDlgItem(IDC_RADIO_QUERY_BY_COORD)->EnableWindow(FALSE);
	}

	// set tile and width for each colum in the list control
	CRect list_ctrl_rc;
	ctrl_list_locations.GetClientRect(list_ctrl_rc);
	ctrl_list_locations.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
	{
		auto span = list_ctrl_rc.Width() / 5;
		ctrl_list_locations.InsertColumn(0, cmn::GetStringRes(IDS_LOCATION_NAME), LVCFMT_LEFT, span);
		ctrl_list_locations.InsertColumn(1, cmn::GetStringRes(IDS_LOCATION_AO), LVCFMT_LEFT, span * 2);
		ctrl_list_locations.InsertColumn(2, cmn::GetStringRes(IDS_LOCATION_ID), LVCFMT_LEFT, span * 2);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void SetLocationDlg::OnBnClickedRadioQueryType()
{
	UpdateData(TRUE);
	EnableControlsByQueryType();
}

void SetLocationDlg::OnBnClickedButtonDoQuery()
{
	UpdateData(TRUE);

	// clear last queries
	ctrl_list_locations.DeleteAllItems();
	locations.clear();

	const auto &api = DataManager::Instance().GetApiCollections().GetApi(api_type);

	if (int_query_type == 0) {
		// query by text
		api.GetProvider().geocodingDirect(cmn::WideChar2MultiByte(str_query_text), locations);
	} else {
		// query by geo-coordinates
		
		auto string_to_float = [](const std::wstring& str, float &val) -> bool {
			try {
				val = std::stof(str);
				return true;
			}
			catch (const std::exception &) {
				val = 0.f;
				return false;
			}
		};

		// check longitude and latitude values
		if (auto lon{ 0.f }; !string_to_float(str_query_lon.GetString(), lon) ||
			lon < -180.f || lon > 180.f) {
			CString msg;
			msg.Format(cmn::GetStringRes(IDS_TMP_INVALID_LON_VALUE), str_query_lon);

			MessageBox(msg, L"Alert");
			return;
		}
		if (auto lat{ 0.f }; !string_to_float(str_query_lat.GetString(), lat) ||
			lat < -90.f || lat > 90.f) {
			CString msg;
			msg.Format(cmn::GetStringRes(IDS_TMP_INVALID_LAT_VALUE), str_query_lat);

			MessageBox(msg, L"Alert");
			return;
		}

		api.GetProvider().geocodingReverse(cmn::WideChar2MultiByte(str_query_lat),
										   cmn::WideChar2MultiByte(str_query_lon),
										   locations);
	}

	if (!locations.empty()) {
		auto idx{ 0 };
		for (const auto &loc : locations) {
			ctrl_list_locations.InsertItem(idx, cmn::MultiByte2WideChar(loc.name).c_str());
			ctrl_list_locations.SetItemText(idx, 1, cmn::MultiByte2WideChar(loc.administrative_ownership).c_str());
			ctrl_list_locations.SetItemText(idx, 2, cmn::MultiByte2WideChar(loc.id).c_str());

			++idx;
		}

		ctrl_list_locations.SetSelectionMark(0);
	}
}

void SetLocationDlg::OnOK()
{
	if (auto idx = ctrl_list_locations.GetSelectionMark();
		idx >= 0 && static_cast<size_t>(idx) < locations.size()) {
		selected_location = locations[idx];
	}

	CDialogEx::OnOK();
}
