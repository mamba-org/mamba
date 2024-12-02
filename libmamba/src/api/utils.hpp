// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTILS_HPP
#define MAMBA_UTILS_HPP

#include <stdexcept>
#include <string>
#include <vector>

#include "tl/expected.hpp"

namespace mamba
{
    class Context;

    namespace detail
    {
        struct other_pkg_mgr_spec;
    }

    namespace pip
    {
        enum class Update : bool
        {
            No = false,
            Yes = true,
        };
    }

    using command_args = std::vector<std::string>;

    tl::expected<command_args, std::runtime_error> install_for_other_pkgmgr(
        const Context& ctx,
        const detail::other_pkg_mgr_spec& other_spec,
        pip::Update update
    );

    void
    populate_context_channels_from_specs(const std::vector<std::string>& raw_matchspecs, Context& context);

}

#endif  // MAMBA_UTILS_HPP
