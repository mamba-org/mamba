#pragma once

#include <stdexcept>
#include <string_view>
#include <iostream>
#include <iomanip>

class mamba_error
    : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

static bool ends_with(const std::string_view& str, const std::string_view& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

static bool starts_with(const std::string_view& str, const std::string_view& prefix)
{
    return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

void to_human_readable_filesize(std::ostream& o, double bytes, std::size_t precision = 0)
{
    const char* sizes[] = { " B", "KB", "MB", "GB", "TB" };
    int order = 0;
    while (bytes >= 1024 && order < (5 - 1)) {
        order++;
        bytes = bytes / 1024;
    }
    o << std::fixed << std::setprecision(precision) << bytes << sizes[order];
}