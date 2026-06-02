#include "DataDef.h"

#include <format>

std::string Location::getFormattedString(bool format_geo_coords/* = true*/) const {
    std::string geo_coord;
    if (format_geo_coords && !longitude.empty() && !latitude.empty()) {
        geo_coord = std::format("[{}, {}]", longitude, latitude);
    }

    auto fmt_string = std::format("{}{}", name, geo_coord);
    if (fmt_string.empty()) {
        fmt_string = "[?]";
    }

    return fmt_string;
}
