#pragma once

#include <string>
#include <yyjson.h>

namespace jvh
{
    inline std::string getString(yyjson_val *j_val, const char *key) {
        if (const auto *s = yyjson_get_str(yyjson_obj_get(j_val, key));
            s != nullptr) {
            return std::string{ s };
        }

        return {};
    }

    inline int getInt(yyjson_val *j_val, const char *key) {
        return yyjson_get_int(yyjson_obj_get(j_val, key));
    }

    inline int64_t getSignedInt(yyjson_val *j_val, const char *key) {
        return yyjson_get_sint(yyjson_obj_get(j_val, key));
    }

    inline uint64_t getUnsignedInt(yyjson_val *j_val, const char *key) {
        return yyjson_get_uint(yyjson_obj_get(j_val, key));
    }

    inline double getNumber(yyjson_val *j_val, const char *key) {
        return yyjson_get_num(yyjson_obj_get(j_val, key));
    }

    inline double getReal(yyjson_val *j_val, const char *key) {
        return yyjson_get_real(yyjson_obj_get(j_val, key));
    }

    inline bool getBool(yyjson_val *j_val, const char *key) {
        return yyjson_get_bool(yyjson_obj_get(j_val, key));
    }

    inline bool hasObject(yyjson_val *j_val, const char *key) {
        return yyjson_obj_get(j_val, key) != nullptr;
    }
}
