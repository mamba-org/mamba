// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_CONDA_URL_HPP
#define MAMBA_SPECS_CONDA_URL_HPP

#include <functional>
#include <string_view>

#include "mamba/specs/error.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/util/url.hpp"

namespace mamba::specs
{
    class CondaURL : private util::URL
    {
        using Base = typename util::URL;

    public:

        using StripScheme = util::detail::StripScheme;
        using Credentials = util::detail::Credentials;
        using Encode = util::detail::Encode;
        using Decode = util::detail::Decode;

        inline static constexpr std::string_view token_prefix = "/t/";

        /** Parse a string url.
         * The url must be percent encoded beforehand.
         * cf. https://en.wikipedia.org/wiki/Percent-encoding
         */
        [[nodiscard]] static auto parse(std::string_view url) -> expected_parse_t<CondaURL>;

        /** Create a local URL. */
        CondaURL() = default;
        explicit CondaURL(util::URL&& url);
        explicit CondaURL(const util::URL& url);

        [[nodiscard]] auto generic() const -> const util::URL&;

        using Base::scheme_is_defaulted;
        using Base::scheme;
        using Base::set_scheme;
        using Base::clear_scheme;
        using Base::has_user;
        using Base::user;
        using Base::set_user;
        using Base::clear_user;
        using Base::has_password;
        using Base::password;
        using Base::set_password;
        using Base::clear_password;
        using Base::authentication;
        using Base::host_is_defaulted;
        using Base::host;
        using Base::set_host;
        using Base::clear_host;
        using Base::port;
        using Base::set_port;
        using Base::clear_port;
        using Base::authority;
        using Base::path;
        using Base::pretty_path;
        using Base::clear_path;
        using Base::query;
        using Base::set_query;
        using Base::clear_query;
        using Base::fragment;
        using Base::set_fragment;
        using Base::clear_fragment;

        /**
         * Set the path from a not encoded value.
         *
         * All '/' are not encoded but interpreted as separators.
         * On windows with a file scheme, the colon after the drive letter is not encoded.
         * A leading '/' is added if abscent.
         * If the path contains only a token, a trailing '/' is added afterwards.
         */
        void set_path(std::string_view path, Encode::yes_type = Encode::yes);

        /** Set the path from an already encoded value.
         *
         * A leading '/' is added if abscent.
         * If the path contains only a token, a trailing '/' is added afterwards.
         */
        void set_path(std::string path, Encode::no_type);

        /**
         * Append a not encoded sub path to the current path.
         *
         * Contrary to `std::filesystem::path::append`, this always append and never replace
         * the current path, even if @p subpath starts with a '/'.
         * All '/' are not encoded but interpreted as separators.
         * If the final path contains only a token, a trailing '/' is added afterwards.
         */
        void append_path(std::string_view path, Encode::yes_type = Encode::yes);

        /**
         * Append a already encoded sub path to the current path.
         *
         * Contrary to `std::filesystem::path::append`, this always append and never replace
         * the current path, even if @p subpath starts with a '/'.
         * If the final path contains only a token, a trailing '/' is added afterwards.
         */
        void append_path(std::string_view path, Encode::no_type);

        /** Return whether a token is set. */
        [[nodiscard]] auto has_token() const -> bool;

        /** Return the Conda token, as delimited with "/t/", or empty if there isn't any. */
        [[nodiscard]] auto token() const -> std::string_view;

        /**
         * Set a token.
         *
         * If the URL already contains one replace it at the same location, otherwise, add it at
         * the beginning of the path.
         */
        void set_token(std::string_view token);

        /** Clear the token and return ``true`` if it exists, otherwise return ``false``. */
        auto clear_token() -> bool;

        /** Return the encoded part of the path without any Conda token, always start with '/'. */
        [[nodiscard]] auto path_without_token(Decode::no_type) const -> std::string_view;

        /** Return the decoded part of the path without any Conda token, always start with '/'. */
        [[nodiscard]] auto path_without_token(Decode::yes_type = Decode::yes) const -> std::string;

        /**
         * Set the path from an already encoded value, without changing the Conda token.
         *
         * A leading '/' is added if abscent.
         */
        void set_path_without_token(std::string_view path, Encode::no_type);

        /**
         * Set the path from an not yet encoded value, without changing the Conda token.
         *
         * A leading '/' is added if abscent.
         */
        void set_path_without_token(std::string_view path, Encode::yes_type = Encode::yes);

        /** Clear the path without changing the Conda token and return ``true`` if it exists. */
        auto clear_path_without_token() -> bool;

        /** Return the platform if part of the URL path. */
        [[nodiscard]] auto platform() const -> std::optional<KnownPlatform>;

        /**
         * Return the platform if part of the URL path, or empty.
         *
         * If a platform is found, it is returned as a view onto the path without normalization
         * (for instance the capitalization isn't changed).
         */
        [[nodiscard]] auto platform_name() const -> std::string_view;

        /** Set the platform if the URL already contains one, or throw an error. */
        void set_platform(KnownPlatform platform);

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

        /** Return the full, exact, encoded URL. */
        [[nodiscard]] auto str(Credentials credentials = Credentials::Hide) const -> std::string;

        /**
         * Return the full decoded url.
         *
         * Due to decoding, the outcome may not be understood by parser and usable to reach an
         * asset.
         * @param strip_scheme If true, remove the scheme and "localhost" on file URI.
         * @param rstrip_path If non-null, remove the given characters at the end of the path.
         * @param credentials If true, hide password and tokens in the decoded string.
         * @param credentials Decide to keep, remove, or hide passwrd, users, and token.
         */
        [[nodiscard]] auto pretty_str(
            StripScheme strip_scheme = StripScheme::no,
            char rstrip_path = 0,
            Credentials credentials = Credentials::Hide
        ) const -> std::string;


    private:

        void set_platform_no_check_input(std::string_view platform);
        void ensure_path_without_token_leading_slash();

        friend auto operator==(const CondaURL&, const CondaURL&) -> bool;
    };

    /** Tuple-like equality of all observable members */
    auto operator==(const CondaURL& a, const CondaURL& b) -> bool;
    auto operator!=(const CondaURL& a, const CondaURL& b) -> bool;

    /** A functional equivalent to ``CondaURL::append_path``. */
    auto operator/(const CondaURL& url, std::string_view subpath) -> CondaURL;
    auto operator/(CondaURL&& url, std::string_view subpath) -> CondaURL;

    namespace conda_url_literals
    {
        auto operator""_cu(const char* str, std::size_t len) -> CondaURL;
    }
}

template <>
struct std::hash<mamba::specs::CondaURL>
{
    auto operator()(const mamba::specs::CondaURL& p) const -> std::size_t;
};

#endif
