#include "AppLocale.h"

namespace tr
{
    static Locale enabled_locale{ Locale::CHINESE_S };
    static std::unordered_map<Locale, std::unordered_map<TextID, std::string>> text_table;

    void init() {
        static bool flag{ false };

        if (flag) {
            return;
        }

        // todo: initialize ...
        text_table[Locale::CHINESE_S] = {
            {TextID::LC_OPENWEATHER, "zh_cn"},
            {TextID::LC_QWEATHER, "zh-hans"},

            {TextID::FMT_AIR_QUALITY, "空气质量"},
            {TextID::FMT_DAT_AFTER_TOMORROW, "后日"},
            {TextID::FMT_HUMIDITY, "湿度"},
            {TextID::FMT_TODAY, "今日"},
            {TextID::FMT_TOMORROW, "明日"},
            {TextID::FMT_UVI, "UV指数"},

            {TextID::FMT_TMP_WIND_DIRECTION, "{}"},
            {TextID::FMT_TMP_WIND_SCALE, "{}级"},

            {TextID::ERR_AUTH_NO_API_HOST, "缺失API host"},
            {TextID::ERR_AUTH_NO_APP_KEY, "缺失API key"},
            {TextID::ERR_AUTH_JWT_NO_PROJ_ID, "缺失项目ID"},
            {TextID::ERR_AUTH_JWT_NO_CRED_ID, "缺失JWT凭据ID"},
            {TextID::ERR_CODE, "错误代码"},
            {TextID::ERR_EXTRACT_DATA_FAILED, "提取数据失败"},
            {TextID::ERR_GEN_KEYPAIR_FAILED, "创建密钥对失败"},
            {TextID::ERR_INTERNET_EMPTY_RESPONSE, "服务器没有返回内容"},
            {TextID::ERR_INVALID_JSON, "解析Json内容失败"},
            {TextID::ERR_JWT_CANNOT_OPEN_PRV_FILE, "无法打开私有密钥文件"},
            {TextID::ERR_NO_LONG_LAT, "缺失经纬度坐标"},
            {TextID::ERR_PARSING_JSON_FAILED, "解析Json内容失败"},
            {TextID::ERR_QUERY_FCW3D_FAILED, "查询3天预报天气失败"},
            {TextID::ERR_QUERY_GEOCODING_FAILED, "查询地理位置失败"},
            {TextID::ERR_QUERY_ONECALL_FAILED, "查询OneCall失败"},
            {TextID::ERR_QUERY_RTAQ_FAILED, "查询实时空气质量失败"},
            {TextID::ERR_QUERY_RTW_FAILED, "查询实时天气失败"},
            {TextID::ERR_QUERY_RTWA_FAILED, "查询天气预警失败"},
            {TextID::ERR_UNKOWN, "未知错误"},
        };

        text_table[Locale::ENGLISH] = {
            {TextID::LC_OPENWEATHER, "en"},
            {TextID::LC_QWEATHER, "en"},

            {TextID::FMT_AIR_QUALITY, "AQ"},
            {TextID::FMT_DAT_AFTER_TOMORROW, "DAT"},
            {TextID::FMT_HUMIDITY, "HUM"},
            {TextID::FMT_TODAY, "TOD"},
            {TextID::FMT_TOMORROW, "TMR"},
            {TextID::FMT_UVI, "UVI"},

            {TextID::FMT_TMP_WIND_DIRECTION, "Wind: {}"},
            {TextID::FMT_TMP_WIND_SCALE, "LV.{}"},

            {TextID::ERR_AUTH_NO_API_HOST, "No API host"},
            {TextID::ERR_AUTH_NO_APP_KEY, "No API key"},
            {TextID::ERR_AUTH_JWT_NO_PROJ_ID, "No project ID"},
            {TextID::ERR_AUTH_JWT_NO_CRED_ID, "No JWT credential ID"},
            {TextID::ERR_CODE, "Error code"},
            {TextID::ERR_EXTRACT_DATA_FAILED, "Failed to extract data from response"},
            {TextID::ERR_GEN_KEYPAIR_FAILED, "Failed to generate a keypair"},
            {TextID::ERR_INTERNET_EMPTY_RESPONSE, "No content returned by server"},
            {TextID::ERR_INVALID_JSON, "Invalid json content"},
            {TextID::ERR_JWT_CANNOT_OPEN_PRV_FILE, "Failed to open private key file"},
            {TextID::ERR_NO_LONG_LAT, "No longitude or latitude coordinate"},
            {TextID::ERR_PARSING_JSON_FAILED, "Invalid json content"},
            {TextID::ERR_QUERY_FCW3D_FAILED, "Failed to query forecasted weather in 3 days"},
            {TextID::ERR_QUERY_GEOCODING_FAILED, "Failed to query location information"},
            {TextID::ERR_QUERY_ONECALL_FAILED, "Failed to query weather by onecall"},
            {TextID::ERR_QUERY_RTAQ_FAILED, "Failed to query realtime air quality"},
            {TextID::ERR_QUERY_RTW_FAILED, "Failed to query realtime weather"},
            {TextID::ERR_QUERY_RTWA_FAILED, "Failed to query weather alerts"},
            {TextID::ERR_UNKOWN, "Unkown error"},
        };

        flag = true;
    }

    void setLocale(Locale l) {
        enabled_locale = l;
    }

    const std::string& txt(TextID id) {
        static const std::string missing{ "<missing text>" };

        auto loc_it = text_table.find(enabled_locale);
        if (loc_it == text_table.end())
            return missing;

        auto text_it = loc_it->second.find(id);
        if (text_it == loc_it->second.end())
            return missing;

        return text_it->second;
    }
}
