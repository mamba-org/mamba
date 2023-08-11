// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <filesystem>
#include <string>

#include "mamba/core/mamba_fs.hpp"
#include "mamba/util/string.hpp"

namespace fs
{

#if defined(_WIN32)
    // Maintain `\` on Windows, `/` on other platforms
    std::filesystem::path normalized_separators(std::filesystem::path path)
    {
        auto native_string = path.native();
        static constexpr auto platform_separator = L"\\";
        static constexpr auto other_separator = L"/";
        mamba::util::replace_all(native_string, other_separator, platform_separator);
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
}
