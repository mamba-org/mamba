#pragma once

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <cassert>

#ifdef _WIN32
#include "thirdparty/string_view.hpp"
namespace std { using nonstd::string_view; }
#else
#include <string_view>
#endif

#include "thirdparty/filesystem.hpp"
namespace fs = ghc::filesystem;

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

inline void to_human_readable_filesize(std::ostream& o, double bytes, std::size_t precision = 0)
{
    const char* sizes[] = { " B", "KB", "MB", "GB", "TB" };
    int order = 0;
    while (bytes >= 1024 && order < (5 - 1)) {
        order++;
        bytes = bytes / 1024;
    }
    o << std::fixed << std::setprecision(precision) << bytes << sizes[order];
}

class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        bool success = false;
        #ifndef _WIN32
            std::string template_path = fs::temp_directory_path() / "mambaXXXXXXX";
            char* pth = mkdtemp((char*)template_path.c_str());
            success = (pth != nullptr);
            template_path = pth;
        #else
            std::string template_path = fs::temp_directory_path() / "mambaXXXXXXX";
            // include \0 terminator
            auto err = _mktemp_s((char*)template_path.c_str(), template_path.size() + 1);
            assert(err == 0);
            success = fs::create_directory(template_path);
        #endif
        if (!success)
        {
            throw std::runtime_error("Could not create temporary directory!");
        }
        else
        {
            m_path = template_path;
        }
    }

    ~TemporaryDirectory()
    {
        fs::remove_all(m_path);
    }

    fs::path& path()
    {
        return m_path;
    }

    operator fs::path()
    {
        return m_path;
    }

private:
    fs::path m_path;
};
