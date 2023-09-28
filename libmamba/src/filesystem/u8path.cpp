// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <filesystem>
#include <string>
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>

// We can use the presence of UTIME_OMIT to detect platforms that provide
// utimensat.
#if defined(UTIME_OMIT)
#define USE_UTIMENSAT
#endif
#endif

#include "mamba/filesystem/u8path.hpp"

namespace mamba::fs
{

#if defined(_WIN32)
    // Maintain `\` on Windows, `/` on other platforms
    std::filesystem::path normalized_separators(std::filesystem::path path)
    {
        auto native_string = path.native();
        static constexpr wchar_t platform_separator = L'\\';
        static constexpr wchar_t other_separator = L'/';

        auto pos = native_string.find(other_separator);
        while (pos != decltype(native_string)::npos)
        {
            native_string[pos] = platform_separator;
            pos = native_string.find(other_separator, pos + 1);
        }

        path = std::move(native_string);
        return path;
    }
#else
    // noop on non-Windows platforms
    std::filesystem::path normalized_separators(std::filesystem::path path)
    {
        return path;
    }
#endif

#if __cplusplus == 201703L
    std::string to_utf8(const std::filesystem::path& path)
    {
        return normalized_separators(path).u8string();
    }

    std::filesystem::path from_utf8(std::string_view u8string)
    {
        return normalized_separators(std::filesystem::u8path(u8string));
    }
#else
#error UTF8 functions implementation is specific to C++17, using another version requires a different implementation.
#endif

    void last_write_time(const u8path& path, now, std::error_code& ec) noexcept
    {
#if defined(USE_UTIMENSAT)
        if (utimensat(AT_FDCWD, path.string().c_str(), NULL, 0) == -1)
        {
            ec = std::error_code(errno, std::generic_category());
        }
#else
        auto new_time = fs::file_time_type::clock::now();
        std::filesystem::last_write_time(path, new_time, ec);
#endif
    }
}
