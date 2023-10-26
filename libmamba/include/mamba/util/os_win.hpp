// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_OS_WIN_HPP
#define MAMBA_UTIL_OS_WIN_HPP

#include <string>
#include <string_view>

#include "mamba/fs/filesystem.hpp"

namespace mamba::util
{
    enum class WindowsKnowUserFolder
    {
        Documents,
        Profile,
        Programs,
        ProgramData,
        LocalAppData,
        RoamingAppData,
    };

    [[nodiscard]] auto get_windows_known_user_folder(WindowsKnowUserFolder dir) -> fs::u8path;

    [[nodiscard]] auto utf8_to_windows_encoding(const std::string_view utf8_text) -> std::wstring;

    [[nodiscard]] auto windows_encoding_to_utf8(std::wstring_view) -> std::string;
}
#endif
