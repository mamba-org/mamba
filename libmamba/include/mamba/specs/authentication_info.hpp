// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_AUTHENTIFICATION_INFO_HPP
#define MAMBA_SPECS_AUTHENTIFICATION_INFO_HPP

#include <string>
#include <unordered_map>
#include <variant>

namespace mamba::specs
{
    class CondaURL;

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

    class AuthenticationDataBase : private std::unordered_map<std::string, AuthenticationInfo>
    {
    public:

        using Base = std::unordered_map<std::string, AuthenticationInfo>;

        using typename Base::key_type;
        using typename Base::mapped_type;
        using typename Base::value_type;
        using typename Base::size_type;
        using typename Base::iterator;
        using typename Base::const_iterator;

        using Base::Base;

        using Base::begin;
        using Base::end;
        using Base::cbegin;
        using Base::cend;

        using Base::empty;
        using Base::size;
        using Base::max_size;

        using Base::clear;
        using Base::insert;
        using Base::insert_or_assign;
        using Base::emplace;
        using Base::emplace_hint;
        using Base::try_emplace;
        using Base::erase;
        using Base::swap;
        using Base::extract;
        using Base::merge;

        using Base::reserve;

        using Base::at;

        [[nodiscard]] auto at_compatible(const key_type& key) -> mapped_type&;
        [[nodiscard]] auto at_compatible(const key_type& key) const -> const mapped_type&;

        using Base::find;

        auto find_compatible(const key_type& key) -> iterator;
        auto find_compatible(const key_type& key) const -> const_iterator;

        [[nodiscard]] auto contains(const key_type& key) const -> bool;

        [[nodiscard]] auto contains_compatible(const key_type& key) const -> bool;
    };
}
#endif
