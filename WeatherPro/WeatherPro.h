// WeatherPro.h: WeatherPro DLL 的主标头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif

#include "PluginInterface.h"
#include "MainItem.h"
#include "PinnedItem.h"

#include <vector>

struct WpVersion
{
	unsigned int major{ 0 };
	unsigned int minor{ 0 };
	unsigned int patch{ 0 };

	auto operator<=>(const WpVersion&) const = default;
	std::wstring to_wstring() const;
};

struct WpLatestPackage
{
	WpVersion version;
	std::wstring url_x86;
	std::wstring url_x64;
};

class WeatherPro final : public ITMPlugin
{
	WeatherPro() = default;

	static WeatherPro instance;
public:
	static WeatherPro& Instance();

	IPluginItem* GetItem(int index) override;
	void DataRequired() override;
	const wchar_t* GetInfo(PluginInfoIndex index) override;
	const wchar_t* GetTooltipInfo() override;
	OptionReturn ShowOptionsDialog(void* hParent) override;
	void OnInitialize(ITrafficMonitor *pApp) override;
	void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;

	void UpdateWeather(bool force = false);

	const WpVersion& GetWpVersion() const;
	const WpLatestPackage& GetWpLatestPackage() const;
	bool HasNewVersionWpPackage() const;
	void CheckNewVersion();

private:
	void InitPinnedItems();
	void UpdateWeather(std::time_t ts, bool force = false);
	void CheckNewVersion(std::time_t ts, bool force = false);

	WpLatestPackage latest_package;
	WpVersion current_version;
	std::wstring str_current_version;

	MainItem main_item;
	std::vector<PinnedItem> pinned_items;

	ITrafficMonitor *host_app{ nullptr };
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
