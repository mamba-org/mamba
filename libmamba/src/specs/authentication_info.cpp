// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/specs/authentication_info.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/tuple_hash.hpp"

namespace mamba::specs
{
    namespace
    {
        auto attrs(const BasicHTTPAuthentication& auth)
        {
            return std::tie(auth.user, auth.password);
        }
    }

    auto operator==(const BasicHTTPAuthentication& a, const BasicHTTPAuthentication& b) -> bool
    {
        return attrs(a) == attrs(b);
    }

    auto operator!=(const BasicHTTPAuthentication& a, const BasicHTTPAuthentication& b) -> bool
    {
        return !(a == b);
    }

    auto operator==(const BearerToken& a, const BearerToken& b) -> bool
    {
        return a.token == b.token;
    }

    auto operator!=(const BearerToken& a, const BearerToken& b) -> bool
    {
        return !(a == b);
    }

    auto operator==(const CondaToken& a, const CondaToken& b) -> bool
    {
        return a.token == b.token;
    }

    auto operator!=(const CondaToken& a, const CondaToken& b) -> bool
    {
        return !(a == b);
    }

    auto URLWeakener::make_first_key(std::string_view key) const -> std::string
    {
        if (util::ends_with(key, '/'))
        {
            return std::string(key);
        }
        return util::concat(key, '/');
    }

    auto URLWeakener::weaken_key(std::string_view key) const -> std::optional<std::string_view>
    {
        const auto pos = key.rfind('/');
        if (key.empty() || (pos == std::string::npos))
        {
            // Nothing else to try
            return std::nullopt;
        }
        else if ((pos + 1) == key.size())
        {
            // Try again without final '/'
            return { key.substr(0, pos) };
        }
        else
        {
            // Try again without final element
            return { key.substr(0, pos + 1) };
        }
    }
}

auto
std::hash<mamba::specs::BasicHTTPAuthentication>::operator()(
    const mamba::specs::BasicHTTPAuthentication& auth
) const -> std::size_t
{
    return mamba::util::hash_tuple(mamba::specs::attrs(auth));
}

auto
std::hash<mamba::specs::BearerToken>::operator()(const mamba::specs::BearerToken& auth
) const -> std::size_t
{
    return std::hash<std::string>{}(auth.token);
}

auto
std::hash<mamba::specs::CondaToken>::operator()(const mamba::specs::CondaToken& auth
) const -> std::size_t
{
    return std::hash<std::string>{}(auth.token);
}
