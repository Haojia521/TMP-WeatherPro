#include "AppLocale.h"

#include <array>
#include <unordered_map>

namespace
{
    using namespace tr;

    auto enabled_locale{ Locale::CHINESE_S };

    struct LocaleTextItem
    {
        TID tid;
        std::string_view text;
    };

    // the order of the items must be consistent with the order of the TID enumeration values
    constexpr std::array<LocaleTextItem, static_cast<size_t>(TID::COUNT)> TEXT_TABLE_ZH_S = std::to_array<LocaleTextItem>(
        {
            {.tid = TID::LC_OPENWEATHER, .text = "zh_cn"},
            {.tid = TID::LC_QWEATHER, .text = "zh-hans"},

            {.tid = TID::FMT_AIR_QUALITY, .text = "空气质量"},
            {.tid = TID::FMT_DAT_AFTER_TOMORROW, .text = "后天"},
            {.tid = TID::FMT_HUMIDITY, .text = "湿度"},
            {.tid = TID::FMT_TODAY, .text = "今天"},
            {.tid = TID::FMT_TOMORROW, .text = "明天"},
            {.tid = TID::FMT_UVI, .text = "UV指数"},

            {.tid = TID::FMT_TMP_WIND, .text = "{}"},
            {.tid = TID::FMT_TMP_WIND_DIRECTION, .text = "{}风"},
            {.tid = TID::FMT_TMP_WIND_SCALE, .text = "{}级"},

            {.tid = TID::FMT_DIRECTION_N, .text = "北"},
            {.tid = TID::FMT_DIRECTION_NNE, .text = "东北偏北"},
            {.tid = TID::FMT_DIRECTION_NE, .text = "东北"},
            {.tid = TID::FMT_DIRECTION_ENE, .text = "东北偏东"},
            {.tid = TID::FMT_DIRECTION_E, .text = "东"},
            {.tid = TID::FMT_DIRECTION_ESE, .text = "东南偏东"},
            {.tid = TID::FMT_DIRECTION_SE, .text = "东南"},
            {.tid = TID::FMT_DIRECTION_SSE, .text = "东南偏南"},
            {.tid = TID::FMT_DIRECTION_S, .text = "南"},
            {.tid = TID::FMT_DIRECTION_SSW, .text = "西南偏南"},
            {.tid = TID::FMT_DIRECTION_SW, .text = "西南"},
            {.tid = TID::FMT_DIRECTION_WSW, .text = "西南偏西"},
            {.tid = TID::FMT_DIRECTION_W, .text = "西"},
            {.tid = TID::FMT_DIRECTION_WNW, .text = "西北偏西"},
            {.tid = TID::FMT_DIRECTION_NW, .text = "西北"},
            {.tid = TID::FMT_DIRECTION_NNW, .text = "西北偏北"},
            {.tid = TID::FMT_DIRECTION_NONE, .text = "无持续方向"},
            {.tid = TID::FMT_DIRECTION_VRB, .text = "方向变化不定"},

            {.tid = TID::FMT_PRECIPITATION, .text = "降水"},
            {.tid = TID::FMT_PRECIPITATION_INTENSITY, .text = "降水强度"},
            {.tid = TID::FMT_PRECIPITATION_IN_1H, .text = "累计1小时降水量"},
            {.tid = TID::FMT_PRECIPITATION_RAIN, .text = "雨"},
            {.tid = TID::FMT_PRECIPITATION_SNOW, .text = "雪"},
            {.tid = TID::FMT_PRECIPITATION_ICE, .text = "冰粒/冻雨"},
            {.tid = TID::FMT_PRECIPITATION_MIXED, .text = "混合"},
            {.tid = TID::FMT_PRECIPITATION_NONE, .text = "无"},
            {.tid = TID::FMT_PRECIPITATION_UNKNOWN, .text = "未知"},

            {.tid = TID::FMT_DAY_TIME, .text = "日"},
            {.tid = TID::FMT_NIGHT_TIME, .text = "夜"},

            {.tid = TID::ERR_AUTH_NO_API_HOST, .text = "缺失API host"},
            {.tid = TID::ERR_AUTH_NO_APP_KEY, .text = "缺失API key"},
            {.tid = TID::ERR_AUTH_JWT_NO_DEV_ID, .text = "缺失开发者ID"},
            {.tid = TID::ERR_AUTH_JWT_NO_PROJ_ID, .text = "缺失项目ID"},
            {.tid = TID::ERR_AUTH_JWT_NO_CRED_ID, .text = "缺失JWT凭据ID"},
            {.tid = TID::ERR_CODE, .text = "错误代码"},
            {.tid = TID::ERR_EXTRACT_DATA_FAILED, .text = "提取数据失败"},
            {.tid = TID::ERR_GEN_KEYPAIR_FAILED, .text = "创建密钥对失败"},
            {.tid = TID::ERR_INTERNET_EMPTY_RESPONSE, .text = "服务器没有返回内容"},
            {.tid = TID::ERR_INVALID_JSON, .text = "解析Json内容失败"},
            {.tid = TID::ERR_JWT_CANNOT_OPEN_PRV_FILE, .text = "无法打开私有密钥文件"},
            {.tid = TID::ERR_NO_LONG_LAT, .text = "缺失经纬度坐标"},
            {.tid = TID::ERR_PARSING_JSON_FAILED, .text = "解析Json内容失败"},
            {.tid = TID::ERR_QUERY_FCW3D_FAILED, .text = "查询3天预报天气失败"},
            {.tid = TID::ERR_QUERY_GEOCODING_FAILED, .text = "查询地理位置失败"},
            {.tid = TID::ERR_QUERY_ONECALL_FAILED, .text = "OneCall查询失败"},
            {.tid = TID::ERR_QUERY_RTAQ_FAILED, .text = "查询实时空气质量失败"},
            {.tid = TID::ERR_QUERY_RTW_FAILED, .text = "查询实时天气失败"},
            {.tid = TID::ERR_QUERY_RTWA_FAILED, .text = "查询天气预警失败"},
            {.tid = TID::ERR_UNKOWN, .text = "未知错误"},
        });

    // the order of the items must be consistent with the order of the TID enumeration values
    constexpr std::array<LocaleTextItem, static_cast<size_t>(TID::COUNT)> TEXT_TABLE_EN = std::to_array<LocaleTextItem>(
        {
            {.tid = TID::LC_OPENWEATHER, .text = "en"},
            {.tid = TID::LC_QWEATHER, .text = "en"},

            {.tid = TID::FMT_AIR_QUALITY, .text = "AQ"},
            {.tid = TID::FMT_DAT_AFTER_TOMORROW, .text = "DAT"},
            {.tid = TID::FMT_HUMIDITY, .text = "HUM"},
            {.tid = TID::FMT_TODAY, .text = "TOD"},
            {.tid = TID::FMT_TOMORROW, .text = "TMR"},
            {.tid = TID::FMT_UVI, .text = "UVI"},

            {.tid = TID::FMT_TMP_WIND, .text = "Wind: {}"},
            {.tid = TID::FMT_TMP_WIND_DIRECTION, .text = "{}"},
            {.tid = TID::FMT_TMP_WIND_SCALE, .text = "LV.{}"},

            {.tid = TID::FMT_DIRECTION_N, .text = "N"},
            {.tid = TID::FMT_DIRECTION_NNE, .text = "NNE"},
            {.tid = TID::FMT_DIRECTION_NE, .text = "NE"},
            {.tid = TID::FMT_DIRECTION_ENE, .text = "ENE"},
            {.tid = TID::FMT_DIRECTION_E, .text = "E"},
            {.tid = TID::FMT_DIRECTION_ESE, .text = "ESE"},
            {.tid = TID::FMT_DIRECTION_SE, .text = "SE"},
            {.tid = TID::FMT_DIRECTION_SSE, .text = "SSE"},
            {.tid = TID::FMT_DIRECTION_S, .text = "S"},
            {.tid = TID::FMT_DIRECTION_SSW, .text = "SSW"},
            {.tid = TID::FMT_DIRECTION_SW, .text = "SW"},
            {.tid = TID::FMT_DIRECTION_WSW, .text = "WSW"},
            {.tid = TID::FMT_DIRECTION_W, .text = "W"},
            {.tid = TID::FMT_DIRECTION_WNW, .text = "WNW"},
            {.tid = TID::FMT_DIRECTION_NW, .text = "NW"},
            {.tid = TID::FMT_DIRECTION_NNW, .text = "NNW"},
            {.tid = TID::FMT_DIRECTION_NONE, .text = "NONE"},
            {.tid = TID::FMT_DIRECTION_VRB, .text = "VRB"},

            {.tid = TID::FMT_PRECIPITATION, .text = "Precipitation"},
            {.tid = TID::FMT_PRECIPITATION_INTENSITY, .text = "Precipitation intensity"},
            {.tid = TID::FMT_PRECIPITATION_IN_1H, .text = "Precipitation amount in 1 hour"},
            {.tid = TID::FMT_PRECIPITATION_RAIN, .text = "Rain"},
            {.tid = TID::FMT_PRECIPITATION_SNOW, .text = "Snow"},
            {.tid = TID::FMT_PRECIPITATION_ICE, .text = "Ice"},
            {.tid = TID::FMT_PRECIPITATION_MIXED, .text = "Mixed"},
            {.tid = TID::FMT_PRECIPITATION_NONE, .text = "None"},
            {.tid = TID::FMT_PRECIPITATION_UNKNOWN, .text = "Unkown"},

            {.tid = TID::FMT_DAY_TIME, .text = "Day"},
            {.tid = TID::FMT_NIGHT_TIME, .text = "Night"},

            {.tid = TID::ERR_AUTH_NO_API_HOST, .text = "No API host"},
            {.tid = TID::ERR_AUTH_NO_APP_KEY, .text = "No API key"},
            {.tid = TID::ERR_AUTH_JWT_NO_DEV_ID, .text = "No developer ID"},
            {.tid = TID::ERR_AUTH_JWT_NO_PROJ_ID, .text = "No project ID"},
            {.tid = TID::ERR_AUTH_JWT_NO_CRED_ID, .text = "No JWT credential ID"},
            {.tid = TID::ERR_CODE, .text = "Error code"},
            {.tid = TID::ERR_EXTRACT_DATA_FAILED, .text = "Failed to extract data from response"},
            {.tid = TID::ERR_GEN_KEYPAIR_FAILED, .text = "Failed to generate a keypair"},
            {.tid = TID::ERR_INTERNET_EMPTY_RESPONSE, .text = "No content returned by server"},
            {.tid = TID::ERR_INVALID_JSON, .text = "Invalid json content"},
            {.tid = TID::ERR_JWT_CANNOT_OPEN_PRV_FILE, .text = "Failed to open private key file"},
            {.tid = TID::ERR_NO_LONG_LAT, .text = "No longitude or latitude coordinate"},
            {.tid = TID::ERR_PARSING_JSON_FAILED, .text = "Invalid json content"},
            {.tid = TID::ERR_QUERY_FCW3D_FAILED, .text = "Failed to query forecasted weather in 3 days"},
            {.tid = TID::ERR_QUERY_GEOCODING_FAILED, .text = "Failed to query location information"},
            {.tid = TID::ERR_QUERY_ONECALL_FAILED, .text = "Failed to query weather by onecall"},
            {.tid = TID::ERR_QUERY_RTAQ_FAILED, .text = "Failed to query realtime air quality"},
            {.tid = TID::ERR_QUERY_RTW_FAILED, .text = "Failed to query realtime weather"},
            {.tid = TID::ERR_QUERY_RTWA_FAILED, .text = "Failed to query weather alerts"},
            {.tid = TID::ERR_UNKOWN, .text = "Unkown error"},
        });

    template<size_t N>
    constexpr bool checkTableTextOrder(const std::array<LocaleTextItem, N> &tab) {
        for (size_t i = 0; i < N; ++i) {
            if (static_cast<size_t>(tab[i].tid) != i) {
                return false;
            }
        }

        return true;
    }

    static_assert(checkTableTextOrder(TEXT_TABLE_ZH_S), "TEXT_TABLE_ZH_S: TID does not match index");
    static_assert(checkTableTextOrder(TEXT_TABLE_EN), "TEXT_TABLE_EN: TID does not match index");

    const auto& getTextTable() {
        switch (enabled_locale) {
        case Locale::CHINESE_S:
            return TEXT_TABLE_ZH_S;

        case Locale::ENGLISH:
            return TEXT_TABLE_EN;
        }

        return TEXT_TABLE_EN;
    }
}

namespace tr
{
    void setLocale(Locale lc) {
        enabled_locale = lc;
    }

    Locale getLocale() {
        return enabled_locale;
    }

    std::string_view txt(TID tid) {
        const auto idx = static_cast<size_t>(tid);
        if (idx >= static_cast<size_t>(TID::COUNT)) {
            return "<Unkown TID>";
        }

        return getTextTable()[idx].text;
    }
}
