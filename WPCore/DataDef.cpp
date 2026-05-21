#include "DataDef.h"

#include <format>

std::string Location::getFormattedString() const {
    std::string geo_coord;
    if (!longitude.empty() && !latitude.empty()) {
        geo_coord = std::format("[{}, {}]", longitude, latitude);
    }

    auto fmt_string = std::format("{}{}", name, geo_coord);
    if (fmt_string.empty()) {
        fmt_string = "[?]";
    }

    return fmt_string;
}
