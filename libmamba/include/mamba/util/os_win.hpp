// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_OS_WIN_HPP
#define MAMBA_UTIL_OS_WIN_HPP

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

    auto get_windows_known_user_folder(WindowsKnowUserFolder dir) -> fs::u8path;
}
#endif
