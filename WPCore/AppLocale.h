#pragma once

#include <string_view>

namespace tr
{
    enum class Locale
    {
        CHINESE_S,
        ENGLISH,
    };

    enum class TID
    {
        LC_OPENWEATHER,
        LC_QWEATHER,

        FMT_AIR_QUALITY,
        FMT_DAT_AFTER_TOMORROW,
        FMT_HUMIDITY,
        FMT_TODAY,
        FMT_TOMORROW,
        FMT_UVI,

        FMT_TMP_WIND_DIRECTION,
        FMT_TMP_WIND_SCALE,

        ERR_AUTH_NO_API_HOST,
        ERR_AUTH_NO_APP_KEY,
        ERR_AUTH_JWT_NO_PROJ_ID,
        ERR_AUTH_JWT_NO_CRED_ID,
        ERR_CODE,
        ERR_EXTRACT_DATA_FAILED,
        ERR_GEN_KEYPAIR_FAILED,
        ERR_INTERNET_EMPTY_RESPONSE,
        ERR_INVALID_JSON,
        ERR_JWT_CANNOT_OPEN_PRV_FILE,
        ERR_NO_LONG_LAT,
        ERR_PARSING_JSON_FAILED,
        ERR_QUERY_FCW3D_FAILED,
        ERR_QUERY_GEOCODING_FAILED,
        ERR_QUERY_ONECALL_FAILED,
        ERR_QUERY_RTAQ_FAILED,
        ERR_QUERY_RTW_FAILED,
        ERR_QUERY_RTWA_FAILED,
        ERR_UNKOWN,

        COUNT,    // must be the last one, dont use it
    };

    void setLocale(Locale);
    Locale getLocale();
    std::string_view txt(TID);
}
