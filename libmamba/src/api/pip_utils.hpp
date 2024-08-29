#ifndef MAMBA_PIP_UTILS_HPP
#define MAMBA_PIP_UTILS_HPP

#include <vector>

// TODO: move elements from `mamba/api/install.hpp` here
#include "mamba/api/install.hpp"
#include "mamba/core/context.hpp"

#include "tl/expected.hpp"

namespace mamba
{
    using command_args = std::vector<std::string>;

    tl::expected<command_args, std::runtime_error>
    install_for_other_pkgmgr(const Context& ctx, const detail::other_pkg_mgr_spec& other_spec, bool update);

}

#endif  // MAMBA_PIP_UTILS_HPP
