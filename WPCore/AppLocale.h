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

        FMT_TMP_WIND,
        FMT_TMP_WIND_DIRECTION,
        FMT_TMP_WIND_SCALE,

        FMT_DIRECTION_N,
        FMT_DIRECTION_NNE,
        FMT_DIRECTION_NE,
        FMT_DIRECTION_ENE,
        FMT_DIRECTION_E,
        FMT_DIRECTION_ESE,
        FMT_DIRECTION_SE,
        FMT_DIRECTION_SSE,
        FMT_DIRECTION_S,
        FMT_DIRECTION_SSW,
        FMT_DIRECTION_SW,
        FMT_DIRECTION_WSW,
        FMT_DIRECTION_W,
        FMT_DIRECTION_WNW,
        FMT_DIRECTION_NW,
        FMT_DIRECTION_NNW,
        FMT_DIRECTION_NONE,
        FMT_DIRECTION_VRB,

        FMT_PRECIPITATION,
        FMT_PRECIPITATION_INTENSITY,
        FMT_PRECIPITATION_IN_1H,
        FMT_PRECIPITATION_RAIN,
        FMT_PRECIPITATION_SNOW,
        FMT_PRECIPITATION_ICE,
        FMT_PRECIPITATION_MIXED,
        FMT_PRECIPITATION_NONE,
        FMT_PRECIPITATION_UNKNOWN,

        FMT_DAY_TIME,
        FMT_NIGHT_TIME,

        ERR_AUTH_NO_API_HOST,
        ERR_AUTH_NO_APP_KEY,
        ERR_AUTH_JWT_NO_DEV_ID,
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
