// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_URL_HPP
#define MAMBA_SPECS_URL_HPP

#include <string_view>

#include "mamba/util/url.hpp"

namespace mamba::specs
{
    class CondaURL : private util::URL
    {
        using Base = typename util::URL;

    public:

        using Base::StripScheme;
        using Base::HidePassword;
        using Base::Encode;
        using Base::Decode;

        using Base::https;
        using Base::localhost;
        inline static constexpr std::string_view token_prefix = "/t/";

        [[nodiscard]] static auto parse(std::string_view url) -> CondaURL;

        /** Create a local URL. */
        CondaURL() = default;

        using Base::scheme;
        using Base::set_scheme;
        using Base::user;
        using Base::set_user;
        using Base::clear_user;
        using Base::password;
        using Base::set_password;
        using Base::clear_password;
        using Base::authentication;
        using Base::host;
        using Base::set_host;
        using Base::clear_host;
        using Base::port;
        using Base::set_port;
        using Base::clear_port;
        using Base::authority;
        using Base::path;
        using Base::set_path;
        using Base::clear_path;
        using Base::query;
        using Base::set_query;
        using Base::clear_query;
        using Base::fragment;
        using Base::set_fragment;
        using Base::clear_fragment;

        /** Return the Conda token, as delimited with "/t/", or empty if there isn't any. */
        [[nodiscard]] auto token() const -> std::string_view;

        /** Set a token if the URL already contains one, or throw an error. */
        void set_token(std::string_view token);

        /** Clear the token if it exists and return ``true``, otherwise return false. */
        auto clear_token() -> bool;

    private:

        explicit CondaURL(URL&& url);
    };
}
#endif