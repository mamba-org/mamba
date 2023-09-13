// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_CONDA_URL_HPP
#define MAMBA_SPECS_CONDA_URL_HPP

#include <string_view>

#include "mamba/specs/platform.hpp"
#include "mamba/util/url.hpp"

namespace mamba::specs
{
    class CondaURL : private util::URL
    {
        using Base = typename util::URL;

    public:

        using StripScheme = util::detail::StripScheme;
        using HideConfidential = util::detail::HideConfidential;
        using Encode = util::detail::Encode;
        using Decode = util::detail::Decode;

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
        using Base::append_path;
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

        /** Clear the token and return ``true`` if it exists, otherwise return ``false``. */
        auto clear_token() -> bool;

        /** Return the platform if part of the URL path. */
        [[nodiscard]] auto platform() const -> std::optional<Platform>;

        /**
         * Return the platform if part of the URL path, or empty.
         *
         * If a platform is found, it is returned as a view onto the path without normalization
         * (for instance the capitalization isn't changed).
         */
        [[nodiscard]] auto platform_name() const -> std::string_view;

        /** Set the platform if the URL already contains one, or throw an error. */
        void set_platform(Platform platform);

        /**
         * Set the platform if the URL already contains one, or throw an error.
         *
         * If the input @p platform is a valid platform, it is inserted as it is into the path
         * (for instance the capitalization isn't changed).
         */
        void set_platform(std::string_view platform);

        /** Clear the token and return true if it exists, otherwise return ``false``. */
        auto clear_platform() -> bool;

        /**
         * Return the encoded package name, or empty otherwise.
         *
         * Package name are at the end of the path and end with a archive extension.
         *
         * @see has_archive_extension
         */
        [[nodiscard]] auto package(Decode::yes_type = Decode::yes) const -> std::string;

        /**
         * Return the decoded package name, or empty otherwise.
         *
         * Package name are at the end of the path and end with a archive extension.
         *
         * @see has_archive_extension
         */
        [[nodiscard]] auto package(Decode::no_type) const -> std::string_view;

        /**
         * Change the package file name with a not encoded value.
         *
         * If a package file name is present, replace it, otherwise add it at the end
         * of the path.
         */
        void set_package(std::string_view pkg, Encode::yes_type = Encode::yes);

        /**
         * Change the package file name with already encoded value.
         *
         * If a package file name is present, replace it, otherwise add it at the end
         * of the path.
         */
        void set_package(std::string_view pkg, Encode::no_type);

        /** Clear the package and return true if it exists, otherwise return ``false``. */
        auto clear_package() -> bool;

        using Base::str;

        /**
         * Return the full decoded url.
         *
         * Due to decoding, the outcome may not be understood by parser and usable to reach an
         * asset.
         * @param strip_scheme If true, remove the scheme and "localhost" on file URI.
         * @param rstrip_path If non-null, remove the given charaters at the end of the path.
         * @param hide_confidential If true, hide password and tokens in the decoded string.
         */
        [[nodiscard]] auto pretty_str(
            StripScheme strip_scheme = StripScheme::no,
            char rstrip_path = 0,
            HideConfidential hide_confidential = HideConfidential::no
        ) const -> std::string;


    private:

        explicit CondaURL(URL&& url);
        void set_platform_no_check_input(std::string_view platform);
    };
}
#endif
