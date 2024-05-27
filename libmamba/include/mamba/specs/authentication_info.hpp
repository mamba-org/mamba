// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_AUTHENTICATION_INFO_HPP
#define MAMBA_SPECS_AUTHENTICATION_INFO_HPP

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "mamba/util/weakening_map.hpp"

namespace mamba::specs
{
    /** User and password authetification set in the URL. */
    struct BasicHTTPAuthentication
    {
        std::string user;
        std::string password;
    };

    auto operator==(const BasicHTTPAuthentication& a, const BasicHTTPAuthentication& b) -> bool;
    auto operator!=(const BasicHTTPAuthentication& a, const BasicHTTPAuthentication& b) -> bool;

    /** HTTP Bearer token set in the request headers. */
    struct BearerToken
    {
        std::string token;
    };

    auto operator==(const BearerToken& a, const BearerToken& b) -> bool;
    auto operator!=(const BearerToken& a, const BearerToken& b) -> bool;

    /** A Conda token set in the URL path. */
    struct CondaToken
    {
        std::string token;
    };

    auto operator==(const CondaToken& a, const CondaToken& b) -> bool;
    auto operator!=(const CondaToken& a, const CondaToken& b) -> bool;

    using AuthenticationInfo = std::variant<BasicHTTPAuthentication, BearerToken, CondaToken>;

    /**
     * The weakener for @ref AuthenticationDataBase.
     */
    struct URLWeakener
    {
        /**
         * Add a trailing '/' if abscent.
         *
         * This lets "mamba.org/" be found by "mamba.org" key.
         */
        [[nodiscard]] auto make_first_key(std::string_view key) const -> std::string;

        /**
         * Remove the last part of the URL path, or simply the trailing slash.
         *
         * For instance, iterations mya follow
         * "mamba.org/p/chan" > "mamba.org/p/" > "mamba.org/p" > "mamba.org/" > "mamba.org".
         */
        [[nodiscard]] auto weaken_key(std::string_view key) const -> std::optional<std::string_view>;
    };

    /**
     * A class that holds the authentication info stored by users.
     *
     * Essentially a map, except that some keys can match multiple queries.
     * For instance "mamba.org/private" should be matched by queries "mamba.org/private",
     * "mamba.org/private/channel", but not "mamba.org/public".
     *
     * A best effort is made to satifiy this with `xxx_compatible`.
     *
     * Future development of this class should aim to replace the map and keys with a
     * `AuthenticationSpec`, that can decide whether or not a URL should benefit from such
     * its authentication.
     * Possibly, a string representation such as "*.mamba.org/private/channel*" could be added
     * to parse users intentions, rather than relying on the assumptions made here.
     */
    using AuthenticationDataBase = util::
        weakening_map<std::unordered_map<std::string, AuthenticationInfo>, URLWeakener>;
}

template <>
struct std::hash<mamba::specs::BasicHTTPAuthentication>
{
    auto operator()(const mamba::specs::BasicHTTPAuthentication& auth) const -> std::size_t;
};

template <>
struct std::hash<mamba::specs::BearerToken>
{
    auto operator()(const mamba::specs::BearerToken& auth) const -> std::size_t;
};

template <>
struct std::hash<mamba::specs::CondaToken>
{
    auto operator()(const mamba::specs::CondaToken& auth) const -> std::size_t;
};

#endif
