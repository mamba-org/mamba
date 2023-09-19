// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_AUTHENTIFICATION_INFO_HPP
#define MAMBA_SPECS_AUTHENTIFICATION_INFO_HPP

#include <string>
#include <variant>

namespace mamba::specs
{
    /** User and password authetification set in the URL. */
    struct BasicHTTPAuthentication
    {
        std::string user;
        std::string password;
    };

    /** HTTP Bearer token set in the request headers. */
    struct BearerToken
    {
        std::string token;
    };

    /** A Conda token set in the URL path. */
    struct CondaToken
    {
        std::string token;
    };

    using AuthenticationInfo = std::variant<BasicHTTPAuthentication, BearerToken, CondaToken>;
}
#endif
