// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CACHE_PATHS_HPP
#define MAMBA_CORE_CACHE_PATHS_HPP

#include <string_view>

namespace mamba::cache_paths
{
    inline constexpr std::string_view conda_pkgs_relative = "conda/pkgs";
    inline constexpr std::string_view cache_relative = "cache";
    inline constexpr std::string_view cache_shards_relative = "cache/shards";
}  // namespace mamba::cache_paths

#endif
