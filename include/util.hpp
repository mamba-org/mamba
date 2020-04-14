#pragma once

#include <stdexcept>
#include <string_view>
#include <iostream>

#define OUTPUT(x)                   \
    if (!Context::instance().quiet) \
        std::cout << x << "\n";

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