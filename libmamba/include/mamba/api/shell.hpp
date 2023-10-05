// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_SHELL_HPP
#define MAMBA_API_SHELL_HPP

#include <string>
#include <string_view>

#include "mamba/core/palette.hpp"
#include "mamba/fs/filesystem.hpp"

namespace mamba
{
    class Context;

    void shell_init(Context& ctx, const std::string& shell_type, const fs::u8path& prefix);
    void shell_deinit(Context& ctx, const std::string& shell_type, const fs::u8path& prefix);
    void shell_reinit(Context& ctx, const fs::u8path& prefix);
    void shell_hook(Context& ctx, const std::string& shell_type);
    void
    shell_activate(Context& ctx, const fs::u8path& prefix, const std::string& shell_type, bool stack);
    void shell_reactivate(Context& ctx, const std::string& shell_type);
    void shell_deactivate(Context& ctx, const std::string& shell_type);
    void shell_enable_long_path_support(Palette palette = Palette::no_color());
}

#endif
