// PinnedItemSettingsDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "afxdialogex.h"
#include "PinnedItemSettingsDlg.h"
#include "resource.h"

#include "Common.h"
#include "DataManager.h"

namespace
{
	constexpr auto array_weather_item = std::to_array(
		{
			WeatherItem::TEMPERATURE,
			WeatherItem::WEATHER_TEXT,
			WeatherItem::HUMIDITY,
			WeatherItem::WIND,
			WeatherItem::AIR_QUALITY,
			WeatherItem::AIR_PM2P5,
			WeatherItem::AIR_PM10,
			WeatherItem::UV_INDEX,
			WeatherItem::PRECIPITATION,
		});
}

// PinnedItemSettingsDlg 对话框

IMPLEMENT_DYNAMIC(PinnedItemSettingsDlg, CDialogEx)

PinnedItemSettingsDlg::PinnedItemSettingsDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_SETTINGS_PINNED_ITEM, pParent)
{

}

PinnedItemSettingsDlg::~PinnedItemSettingsDlg()
{
}

void PinnedItemSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_TIME_SLOT, ctrl_combo_time_slot);
	DDX_Control(pDX, IDC_COMBO_WEATHER_ITEM, ctrl_combo_weather_item);
	DDX_Control(pDX, IDC_LIST_PINNED_ITEM_KEYS, ctrl_list_current_keys);
}


BEGIN_MESSAGE_MAP(PinnedItemSettingsDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_ADD_KEY, &PinnedItemSettingsDlg::OnBnClickedButtonAddKey)
	ON_BN_CLICKED(IDC_BUTTON_REMOVE_KEY, &PinnedItemSettingsDlg::OnBnClickedButtonRemoveKey)
	ON_BN_CLICKED(IDC_BUTTON_CLEAR_KEYS, &PinnedItemSettingsDlg::OnBnClickedButtonClearKeys)
END_MESSAGE_MAP()


// PinnedItemSettingsDlg 消息处理程序

BOOL PinnedItemSettingsDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	ctrl_combo_time_slot.AddString(cmn::GetString(WeatherTimeSlot::REALTIME));
	ctrl_combo_time_slot.AddString(cmn::GetString(WeatherTimeSlot::TODAY));
	ctrl_combo_time_slot.AddString(cmn::GetString(WeatherTimeSlot::TOMMROW));
	ctrl_combo_time_slot.AddString(cmn::GetString(WeatherTimeSlot::DAY_AFTER_TOMMROW));
	ctrl_combo_time_slot.SetCurSel(0);

	for (const auto &k : array_weather_item) {
		ctrl_combo_weather_item.AddString(cmn::GetString(k));
	}
	//ctrl_combo_weather_item.AddString(cmn::GetString(WeatherItem::TEMPERATURE));
	//ctrl_combo_weather_item.AddString(cmn::GetString(WeatherItem::WEATHER_TEXT));
	//ctrl_combo_weather_item.AddString(cmn::GetString(WeatherItem::HUMIDITY));
	//ctrl_combo_weather_item.AddString(cmn::GetString(WeatherItem::WIND));
	//ctrl_combo_weather_item.AddString(cmn::GetString(WeatherItem::AIR_QUALITY));
	//ctrl_combo_weather_item.AddString(cmn::GetString(WeatherItem::AIR_PM2P5));
	//ctrl_combo_weather_item.AddString(cmn::GetString(WeatherItem::AIR_PM10));
	//ctrl_combo_weather_item.AddString(cmn::GetString(WeatherItem::UV_INDEX));
	//ctrl_combo_weather_item.AddString(cmn::GetString(WeatherItem::PRECIPITATION));
	ctrl_combo_weather_item.SetCurSel(0);

	
	CRect rect;
	ctrl_list_current_keys.GetClientRect(rect);
	ctrl_list_current_keys.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
	
	auto width = rect.Width() / 2;
	ctrl_list_current_keys.InsertColumn(0, cmn::GetStringRes(IDS_TIME_SLOT), LVCFMT_LEFT, width);
	ctrl_list_current_keys.InsertColumn(1, cmn::GetStringRes(IDS_DATA_ITEM), LVCFMT_LEFT, width);

	const auto &pinned_items_keys = DataManager::Instance().GetConfig().pinned_item_data_keys;
	auto idx{ 0 };
	for (const auto &k : pinned_items_keys) {
		current_keys.push_back(k);
		ctrl_list_current_keys.InsertItem(idx, cmn::GetString(k.time_slot));
		ctrl_list_current_keys.SetItemText(idx, 1, cmn::GetString(k.item));

		++idx;
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void PinnedItemSettingsDlg::OnOK()
{
	std::unordered_set<WeatherDataKey, WeatherDataKeyHash> key_set{
		current_keys.begin(), current_keys.end()
	};

	auto &data_mgr = DataManager::Instance();
	const auto &old_keys = data_mgr.GetConfig().pinned_item_data_keys;

	if (key_set != old_keys) {
		DataManager::Config cfg{ data_mgr.GetConfig() };
		cfg.pinned_item_data_keys = key_set;

		data_mgr.SetConfig(cfg);
		data_mgr.SaveConfigs();
	}

	CDialogEx::OnOK();
}

void PinnedItemSettingsDlg::OnBnClickedButtonAddKey()
{
	const auto selected_item_idx = ctrl_combo_weather_item.GetCurSel();
	if (selected_item_idx < 0 || selected_item_idx >= array_weather_item.size()) {
		return;
	}

	WeatherDataKey key{
		.time_slot = static_cast<WeatherTimeSlot>(ctrl_combo_time_slot.GetCurSel()),
		.item = array_weather_item[static_cast<size_t>(selected_item_idx)],
	};

	if (std::find(current_keys.begin(), current_keys.end(), key) == current_keys.end()) {
		auto idx = static_cast<int>(current_keys.size());
		ctrl_list_current_keys.InsertItem(idx, cmn::GetString(key.time_slot));
		ctrl_list_current_keys.SetItemText(idx, 1, cmn::GetString(key.item));

		current_keys.push_back(key);
	}
}

void PinnedItemSettingsDlg::OnBnClickedButtonRemoveKey()
{
	auto idx = ctrl_list_current_keys.GetNextItem(-1, LVNI_SELECTED);
	if (idx >= 0 && idx < current_keys.size()) {
		current_keys.erase(current_keys.begin() + idx);
		ctrl_list_current_keys.DeleteItem(idx);
	}
}

void PinnedItemSettingsDlg::OnBnClickedButtonClearKeys()
{
	current_keys.clear();
	ctrl_list_current_keys.DeleteAllItems();
}
