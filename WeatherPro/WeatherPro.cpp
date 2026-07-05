// WeatherPro.cpp: 定义 DLL 的初始化例程。
//

#include "pch.h"
#include "framework.h"
#include "WeatherPro.h"
#include "resource.h"

#include "Common.h"
#include "DataManager.h"
#include "IconResources.h"
#include "MainSettingsDlg.h"

#include <dependencies/simpleini/SimpleIni.h>
#include <dependencies/yyjson/src/yyjson.h>

#include <WPCore/AppLocale.h>
#include <WPCore/utils.h>

#include <filesystem>
#include <future>

//#ifdef _DEBUG
//#define new DEBUG_NEW
//#endif

//////////////////////////////////////////////////////////////

namespace
{
    std::time_t last_update_timestamp{ 0 };
    std::time_t next_update_span_seconds{ 0 };

    std::time_t CalcUpdateIntervalSeconds(std::time_t t, DataManager::UpdateInterval interval) {
        std::srand(static_cast<unsigned int>(t));
        auto rand_number = std::rand();

        switch (interval) {
            case DataManager::UpdateInterval::Minutes5:
                // next update will be in 5 to 10 minutes
                return static_cast<std::time_t>(5 * 60 + rand_number % (5 * 60));

            case DataManager::UpdateInterval::Minutes15:
                // next update will be in 15 to 20 minutes
                return static_cast<std::time_t>(15 * 60 + rand_number % (5 * 60));

            case DataManager::UpdateInterval::Minutes30:
                // next update will be in 25 to 35 minutes
                return static_cast<std::time_t>(25 * 60 + rand_number % (10 * 60));

            case DataManager::UpdateInterval::Minutes60:
                // next update will be in 50 to 65 minutes
                return static_cast<std::time_t>(50 * 60 + rand_number % (15 * 60));

            case DataManager::UpdateInterval::Minutes120:
                // next update will be in 100 to 120 minutes
                return static_cast<std::time_t>(100 * 60 + rand_number % (20 * 60));
        }

        // default
        // next update will be in 25 to 35 minutes
        return static_cast<std::time_t>(25 * 60 + rand_number % (10 * 60));
    }

    constexpr WORD LANGID_EN_US{ MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US) };
    constexpr WORD LANGID_ZH_CN{ MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED) };

    void SetLanguageId(WORD lang_id) {
        if (lang_id == 0 || lang_id == LOCALE_INVARIANT) {
            // 跟随系统  LOCALE_INVARIANT=127
            SetLanguageId(GetThreadUILanguage());
            return;
        }

        if (PRIMARYLANGID(lang_id) == LANG_CHINESE) {
            SetThreadUILanguage(LANGID_ZH_CN);
            tr::setLocale(tr::Locale::CHINESE_S);
        } else {
            SetThreadUILanguage(LANGID_EN_US);
            tr::setLocale(tr::Locale::ENGLISH);
        }
    }

    void SetLanguageIdLegacy(const std::wstring &cfg_dir) {
        namespace fs = std::filesystem;

        fs::path cfg_dir_path = fs::absolute(cfg_dir).lexically_normal();
        if (cfg_dir_path.filename().empty()) {
            cfg_dir_path = cfg_dir_path.parent_path();
        }

        if (!fs::is_directory(cfg_dir_path)) {
            SetLanguageId(0);
            return;
        }

        // 查找TrafficMonitor的配置文件，读取语言id
        long tm_lang_id{ -1 };
        auto tm_cfg_filepath = cfg_dir_path.parent_path() / L"config.ini";
        if (fs::is_regular_file(tm_cfg_filepath)) {
            CSimpleIniW tm_ini;
            tm_ini.SetUnicode();
            if (tm_ini.LoadFile(tm_cfg_filepath.c_str()) == SI_OK) {
                tm_lang_id = tm_ini.GetLongValue(L"general", L"language", 0);
            }
        }

        if (tm_lang_id <= 0) {           // 没有查找到TrafficMonitor的语言设置 或 语言跟随系统
            SetLanguageId(0);
        } else {
            if (tm_lang_id < 5) {        // 兼容旧版id值 1-English 2-Chinese_S 3-Chinese_T
                if (tm_lang_id == 2 || tm_lang_id == 3) {
                    SetLanguageId(LANGID_ZH_CN);
                } else {
                    SetLanguageId(LANGID_EN_US);
                }
            } else {                     // 新版直接存储语言id
                SetLanguageId(static_cast<WORD>(tm_lang_id));
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // 版本号

    std::time_t last_new_version_checking_timestamp{ 0 };
    constexpr std::time_t NEW_VERSION_CHECKING_INTERVAL_S{ 12 * 60 * 60 };    // checking new version in every 12 hours

    HMODULE GetCurrentModuleHandle()
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());

        HMODULE hModule = nullptr;

        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetCurrentModuleHandle),
            &hModule);

        return hModule;
    }

    WpVersion GetWpVerionFromRc(HMODULE hModule) {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());

        WpVersion wp_version;

        HRSRC hRes = FindResourceW(
            hModule,
            MAKEINTRESOURCEW(VS_VERSION_INFO),
            RT_VERSION);

        if (!hRes)
            return wp_version;

        HGLOBAL hGlobal = LoadResource(hModule, hRes);
        if (!hGlobal)
            return wp_version;

        void* pData = LockResource(hGlobal);
        if (!pData)
            return wp_version;

        VS_FIXEDFILEINFO* pFileInfo = nullptr;
        UINT len = 0;

        if (!VerQueryValueW(pData,
                            L"\\",
                            reinterpret_cast<LPVOID*>(&pFileInfo),
                            &len)) {
            return wp_version;
        }

        if (!pFileInfo) {
            return wp_version;
        }
            
        WORD major = HIWORD(pFileInfo->dwProductVersionMS);
        WORD minor = LOWORD(pFileInfo->dwProductVersionMS);
        WORD patch = HIWORD(pFileInfo->dwProductVersionLS);

        wp_version.major = major;
        wp_version.minor = minor;
        wp_version.patch = patch;

        return wp_version;
    }

    struct WpLatestVersionInfo
    {
        WpVersion version;
        std::string github_url_x86;
        std::string github_url_x64;
        std::string gitee_url_x86;
        std::string gitee_url_x64;
    };

    WpLatestVersionInfo FetchWpLatestVersionInfo(const std::string &host, const std::string &path) {
        WpLatestVersionInfo wp_version_info;

        std::string content;
        auto status_code = utils::internetGet(host, path, content);
        if (status_code == 200 && !content.empty()) {
            std::unique_ptr<yyjson_doc, void(*)(yyjson_doc*)> doc(
                yyjson_read(content.c_str(), content.size(), 0),
                [](yyjson_doc *p) { yyjson_doc_free(p); }
            );

            if (doc != nullptr) {
                auto *root = yyjson_doc_get_root(doc.get());

                if (auto *obj_version = yyjson_obj_get(root, "version");
                    obj_version != nullptr) {
                    wp_version_info.version.major = static_cast<unsigned>(yyjson_get_int(yyjson_obj_get(obj_version, "major")));
                    wp_version_info.version.minor = static_cast<unsigned>(yyjson_get_int(yyjson_obj_get(obj_version, "minor")));
                    wp_version_info.version.patch = static_cast<unsigned>(yyjson_get_int(yyjson_obj_get(obj_version, "patch")));
                }

                if (auto *obj_github = yyjson_obj_get(root, "github");
                    obj_github != nullptr) {
                    wp_version_info.github_url_x86 = yyjson_get_str(yyjson_obj_get(obj_github, "url_x86"));
                    wp_version_info.github_url_x64 = yyjson_get_str(yyjson_obj_get(obj_github, "url_x64"));
                }

                if (auto *obj_gitee = yyjson_obj_get(root, "gitee");
                    obj_gitee != nullptr) {
                    wp_version_info.gitee_url_x86 = yyjson_get_str(yyjson_obj_get(obj_gitee, "url_x86"));
                    wp_version_info.gitee_url_x64 = yyjson_get_str(yyjson_obj_get(obj_gitee, "url_x64"));
                }
            }
        }

        return wp_version_info;
    }

    WpLatestPackage CheckNewVersionFromGithub() {
        static const std::string url_host{ "https://raw.githubusercontent.com" };
        static const std::string url_path{ "/Haojia521/TMP-WeatherPro/refs/heads/main/version.json" };

        auto version_info = FetchWpLatestVersionInfo(url_host, url_path);
        return WpLatestPackage{
            .version = version_info.version,
            .url_x86 = cmn::MultiByte2WideChar(version_info.github_url_x86),
            .url_x64 = cmn::MultiByte2WideChar(version_info.github_url_x64),
        };
    }

    WpLatestPackage CheckNewVersionFromGitee() {
        static const std::string url_host{ "https://raw.giteeusercontent.com" };
        static const std::string url_path{ "/Haojia521/TMP-WeatherPro/raw/main/version.json" };

        auto version_info = FetchWpLatestVersionInfo(url_host, url_path);
        return WpLatestPackage{
            .version = version_info.version,
            .url_x86 = cmn::MultiByte2WideChar(version_info.gitee_url_x86),
            .url_x64 = cmn::MultiByte2WideChar(version_info.gitee_url_x64),
        };
    }

    WpLatestPackage GetWpLatestPackage() {
        std::promise<WpLatestPackage> promise;
        auto future = promise.get_future();

        std::atomic_bool done{ false };
        std::atomic_int finished_count{ 0 };

        constexpr int task_count = 2;

        auto worker = [&](auto fetch_func)
        {
            auto data = fetch_func();

            if (data.version != WpVersion{}) {
                if (auto expected{ false }; done.compare_exchange_strong(expected, true)) {
                    promise.set_value(std::move(data));
                }
            }

            if (++finished_count == task_count) {
                if (auto expected{ false }; done.compare_exchange_strong(expected, true)) {
                    promise.set_exception(
                        std::make_exception_ptr(
                            std::runtime_error("all fetch tasks failed")
                        )
                    );
                }
            }
        };

        std::jthread t1(worker, CheckNewVersionFromGithub);
        std::jthread t2(worker, CheckNewVersionFromGitee);

        try {
            return future.get();
        }
        catch (...) {
            return {};
        }
    }
}

std::wstring WpVersion::to_wstring() const {
    return std::format(L"{}.{}.{}", major, minor, patch);
}

WeatherPro WeatherPro::instance;

WeatherPro& WeatherPro::Instance() {
	return instance;
}

IPluginItem* WeatherPro::GetItem(int index) {
    if (index == 0) {
        return &main_item;
    } else if (index > 0 && index <= static_cast<int>(pinned_items.size())) {
        return &pinned_items[static_cast<size_t>(index - 1)];
    }
    
    return nullptr;
}

void WeatherPro::DataRequired() {
    auto t = std::time(nullptr);
    UpdateWeather(t);
    CheckNewVersion(t);

    if (host_app != nullptr) {
        main_item.SetTaskbarWndDPI(host_app->GetDPI(ITrafficMonitor::DPI_TASKBAR));
    }
}

const wchar_t* WeatherPro::GetInfo(PluginInfoIndex index) {
    switch (index)
    {
        case TMI_NAME:
            return L"WeatherPro";
        case TMI_DESCRIPTION:
            return cmn::GetStringRes(IDS_WP_DESCRIPTION).GetString();
        case TMI_AUTHOR:
            return L"Haojia521";
        case TMI_COPYRIGHT:
            return L"Copyright (C) by Haojia 2025-2026";
        case TMI_URL:
            return L"https://github.com/Haojia521/TMP-WeatherPro";
        case TMI_VERSION:
            return str_current_version.c_str();
        case TMI_MAX:
            break;
    }

    return L"";
}

const wchar_t* WeatherPro::GetTooltipInfo() {
    if (const auto &data_snapshot = DataManager::GetSnapshot();
        DataManager::Instance().GetConfig().show_weather_in_tooltip &&
        data_snapshot != nullptr) {
        return data_snapshot->GetTooltipText();
    }

    return L"";
}

void WeatherPro::OnInitialize(ITrafficMonitor *pApp) {
    //winrt::init_apartment();

    // load icon resources
    // IconSheetManager::Instance().LoadIconResources();
    
    if (pApp != nullptr) {
        // load configs
        //DataManager::Instance().LoadConfigs(pApp->GetPluginConfigDir());

        // set dpi value
        // main_item.SetTaskbarWndDPI(pApp->GetDPI(ITrafficMonitor::DPI_TASKBAR));

        // set thread ui language
        SetLanguageId(pApp->GetLanguageId());

        host_app = pApp;
    }
}

void WeatherPro::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) {
    if (index == ExtendedInfoIndex::EI_TASKBAR_WND_VALUE_RIGHT_ALIGN) {
        try {
            main_item.SetTextAlignRight(std::stoi(data) > 0);
        }
        catch (...) {
            // do nothing
        }
    }

    if (index == ExtendedInfoIndex::EI_CONFIG_DIR) {
        SetLanguageIdLegacy(data);
        IconManager::Instance().LoadIconResources();
        DataManager::Instance().LoadConfigs(data);
        InitPinnedItems();

        current_version = GetWpVerionFromRc(GetCurrentModuleHandle());
        str_current_version = current_version.to_wstring();
    }
}

const WpVersion& WeatherPro::GetWpVersion() const {
    return current_version;
}

const WpLatestPackage WeatherPro::GetWpLatestPackage() const {
    return latest_package;
}

bool WeatherPro::HasNewVersionWpPackage() const {
    return latest_package.version > current_version;
}

void WeatherPro::CheckNewVersion(std::time_t ts) {
    if (ts > (last_new_version_checking_timestamp + NEW_VERSION_CHECKING_INTERVAL_S)) {
        last_new_version_checking_timestamp = ts;

        auto worker = [this]() {
            latest_package = ::GetWpLatestPackage();
        };

        std::thread working_thread(worker);
        working_thread.detach();
    }
}

ITMPlugin::OptionReturn WeatherPro::ShowOptionsDialog(void* hParent) {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    MainSettingsDlg dlg(CWnd::FromHandle(static_cast<HWND>(hParent)));
    if (dlg.DoModal() == IDOK)
        return ITMPlugin::OR_OPTION_CHANGED;
    else
        return ITMPlugin::OR_OPTION_UNCHANGED;
}

void WeatherPro::UpdateWeather(bool force /*= false*/) {
    UpdateWeather(std::time(nullptr), force);
}

void WeatherPro::UpdateWeather(std::time_t ts, bool force /*= false*/) {
    if (force || ts > (last_update_timestamp + next_update_span_seconds)) {
        auto &data_mgr = DataManager::Instance();

        last_update_timestamp = ts;
        next_update_span_seconds =
            CalcUpdateIntervalSeconds(ts, data_mgr.GetConfig().update_interval);

        data_mgr.UpdateWeather();
    }
}

void WeatherPro::InitPinnedItems() {
    const auto &keys = DataManager::Instance().GetConfig().pinned_item_data_keys;

    for (const auto &k : keys) {
        pinned_items.emplace_back(k.time_slot, k.item);
    }
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState())
    return &WeatherPro::Instance();
}
