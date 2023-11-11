// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_ENVIRONMENT_HPP
#define MAMBA_UTIL_ENVIRONMENT_HPP

#include <optional>
#include <string>
#include <unordered_map>

namespace mamba::util
{
    /**
     * Get an environment variable encoded in UTF8.
     */
    [[nodiscard]] auto get_env(const std::string& key) -> std::optional<std::string>;

    /**
     * Set an environment variable encoded in UTF8.
     */
    void set_env(const std::string& key, const std::string& value);

    /**
     * Unset an environment variable encoded in UTF8.
     */
    void unset_env(const std::string& key);

    using environment_map = std::unordered_map<std::string, std::string>;

    /**
     * Return a map of all environment variables encoded in UTF8.
     *
     * This is useful if one in intersted to do an operatrion over all envrionment variables
     * when their name is unknown.
     */
    [[nodiscard]] auto get_env_map() -> environment_map;

    /**
     * Equivalent to calling set_env in a loop.
     *
     * This leaves environment variables not refered to in the map unmodified.
     */
    void update_env_map(const environment_map& env);

    /**
     * Set the environment to be exactly the map given.
     *
     * This unset all environment variables not refered to in the map unmodified.
     */
    void set_env_map(const environment_map& env);
}
#endif
