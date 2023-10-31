// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_ENVIRONMENT_HPP
#define MAMBA_UTIL_ENVIRONMENT_HPP

#include <optional>
#include <string>

namespace mamba::util
{
    auto get_env(const std::string& key) -> std::optional<std::string>;
    void set_env(const std::string& key, const std::string& value);
    void unset_env(const std::string& key);
}
#endif
