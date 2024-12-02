// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_URL_HPP
#define MAMBA_UTIL_URL_HPP

#include <array>
#include <functional>
#include <string>
#include <string_view>

#include <tl/expected.hpp>

namespace mamba::util
{
    namespace detail
    {
        // Working around MSVC limitation on private inheritance + using directive

        enum class StripScheme : bool
        {
            no,
            yes
        };

        enum class Credentials
        {
            Show,
            Hide,
            Remove,
        };

        struct Encode
        {
            inline static constexpr struct yes_type
            {
            } yes = {};

            inline static constexpr struct no_type
            {
            } no = {};
        };

        struct Decode
        {
            inline static constexpr struct yes_type
            {
            } yes = {};

            inline static constexpr struct no_type
            {
            } no = {};
        };
    }

    /**
     * Class representing a URL.
     *
     * All URL have a non-empty scheme, host, and path.
     */
    class URL
    {
    public:

        using StripScheme = detail::StripScheme;
        using Credentials = detail::Credentials;
        using Encode = detail::Encode;
        using Decode = detail::Decode;

        struct ParseError
        {
            std::string what;
        };

        inline static constexpr std::string_view https = "https";
        inline static constexpr std::string_view localhost = "localhost";

        /**
         * Create a URL from a string.
         *
         * The fields of the URL must be percent encoded, otherwise use the individual
         * field setters to encode.
         * For instance, "https://user@email.com@mamba.org/" must be passed as
         *"https://user%40email.com@mamba.org/".The first '@' character is part of the username
         * "user@email.com" whereas the second is the URL specification for separating username
         * and hostname.
         *
         * @see Encode
         * @see mamba::util::url_encode
         */
        [[nodiscard]] static auto parse(std::string_view url) -> tl::expected<URL, ParseError>;

        /** Create a local URL. */
        URL() = default;

        /** Return whether the scheme is defaulted, i.e. not explicitly set. */
        [[nodiscard]] auto scheme_is_defaulted() const -> bool;

        /** Return the scheme, always non-empty. */
        [[nodiscard]] auto scheme() const -> std::string_view;

        /** Set a non-empty scheme. */
        void set_scheme(std::string_view scheme);

        /** Clear the scheme back to a defaulted value and return the old value. */
        auto clear_scheme() -> std::string;

        /** Return whether the user is empty. */
        [[nodiscard]] auto has_user() const -> bool;

        /** Return the encoded user, or empty if none. */
        [[nodiscard]] auto user(Decode::no_type) const -> const std::string&;

        /** Return the decoded user, or empty if none. */
        [[nodiscard]] auto user(Decode::yes_type = Decode::yes) const -> std::string;

        /** Set the user from a not encoded value. */
        void set_user(std::string_view user, Encode::yes_type = Encode::yes);

        /** Set the user from an already encoded value. */
        void set_user(std::string user, Encode::no_type);

        /** Clear and return the encoded user. */
        auto clear_user() -> std::string;

        /** Return whether the password is empty. */
        [[nodiscard]] auto has_password() const -> bool;

        /** Return the encoded password, or empty if none. */
        [[nodiscard]] auto password(Decode::no_type) const -> const std::string&;

        /** Return the decoded password, or empty if none. */
        [[nodiscard]] auto password(Decode::yes_type = Decode::yes) const -> std::string;

        /** Set the password from a not encoded value. */
        void set_password(std::string_view password, Encode::yes_type = Encode::yes);

        /** Set the password from an already encoded value. */
        void set_password(std::string password, Encode::no_type);

        /** Clear and return the encoded password. */
        auto clear_password() -> std::string;

        /** Return the encoded basic authentication string. */
        [[nodiscard]] auto authentication() const -> std::string;

        /** Return whether the host was defaulted, i.e. not explicitly set. */
        [[nodiscard]] auto host_is_defaulted() const -> bool;

        /** Return the encoded host, always non-empty except for file scheme. */
        [[nodiscard]] auto host(Decode::no_type) const -> std::string_view;

        /** Return the decoded host, always non-empty except for file scheme. */
        [[nodiscard]] auto host(Decode::yes_type = Decode::yes) const -> std::string;

        /** Set the host from a not encoded value. */
        void set_host(std::string_view host, Encode::yes_type = Encode::yes);

        /** Set the host from an already encoded value. */
        void set_host(std::string host, Encode::no_type);

        /** Clear and return the encoded hostname. */
        auto clear_host() -> std::string;

        /** Return the port, or empty if none. */
        [[nodiscard]] auto port() const -> const std::string&;

        /** Set or clear the port. */
        void set_port(std::string_view port);

        /** Clear and return the port number. */
        auto clear_port() -> std::string;

        /** Return the encoded authority part of the URL. */
        [[nodiscard]] auto authority(Credentials = Credentials::Hide) const -> std::string;

        /** Return the encoded path, always starts with a '/'. */
        [[nodiscard]] auto path(Decode::no_type) const -> const std::string&;

        /** Return the decoded path, always starts with a '/'. */
        [[nodiscard]] auto path(Decode::yes_type = Decode::yes) const -> std::string;

        /**
         * Set the path from a not encoded value.
         *
         * All '/' are not encoded but interpreted as separators.
         * On windows with a file scheme, the colon after the drive letter is not encoded.
         * A leading '/' is added if abscent.
         */
        void set_path(std::string_view path, Encode::yes_type = Encode::yes);

        /** Set the path from an already encoded value, a leading '/' is added if abscent. */
        void set_path(std::string path, Encode::no_type);

        /** Clear the path and return the encoded path, always starts with a '/'. */
        auto clear_path() -> std::string;

        /**
         * Return the decoded path.
         *
         * For a "file" scheme, with a Windows path containing a drive, the leading '/' is
         * stripped.
         */
        [[nodiscard]] auto pretty_path() const -> std::string;

        /**
         * Append a not encoded sub path to the current path.
         *
         * Contrary to `std::filesystem::path::append`, this always append and never replace
         * the current path, even if @p subpath starts with a '/'.
         * All '/' are not encoded but interpreted as separators.
         */
        void append_path(std::string_view path, Encode::yes_type = Encode::yes);

        /**
         * Append a already encoded sub path to the current path.
         *
         * Contrary to `std::filesystem::path::append`, this always append and never replace
         * the current path, even if @p subpath starts with a '/'.
         */
        void append_path(std::string_view path, Encode::no_type);

        /** Return the query, or empty if none. */
        [[nodiscard]] auto query() const -> const std::string&;

        /** Set or clear the query. */
        void set_query(std::string_view query);

        /** Clear and return the query. */
        auto clear_query() -> std::string;

        /** Return the fragment, or empty if none. */
        [[nodiscard]] auto fragment() const -> const std::string&;

        /** Set or clear the fragment. */
        void set_fragment(std::string_view fragment);

        /** Clear and return the fragment. */
        auto clear_fragment() -> std::string;

        /** Return the full, exact, encoded URL. */
        [[nodiscard]] auto str(Credentials credentials = Credentials::Hide) const -> std::string;

        /**
         * Return the full decoded url.
         *
         * Due to decoding, the outcome may not be understood by parser and usable to fetch the URL.
         * @param strip_scheme If true, remove the scheme and "localhost" on file URI.
         * @param rstrip_path If non-null, remove the given characters at the end of the path.
         * @param credentials Decide to keep, remove, or hide credentials.
         */
        [[nodiscard]] auto pretty_str(
            StripScheme strip_scheme = StripScheme::no,
            char rstrip_path = 0,
            Credentials credentials = Credentials::Hide
        ) const -> std::string;

    protected:

        [[nodiscard]] auto authentication_elems(Credentials, Decode::no_type) const
            -> std::array<std::string_view, 3>;
        [[nodiscard]] auto authentication_elems(Credentials, Decode::yes_type) const
            -> std::array<std::string, 3>;

        [[nodiscard]] auto authority_elems(Credentials, Decode::no_type) const
            -> std::array<std::string_view, 7>;
        [[nodiscard]] auto authority_elems(Credentials, Decode::yes_type) const
            -> std::array<std::string, 7>;

        [[nodiscard]] auto
        pretty_str_path(StripScheme strip_scheme = StripScheme::no, char rstrip_path = 0) const
            -> std::string;

    private:

        std::string m_scheme = {};
        std::string m_user = {};
        std::string m_password = {};
        std::string m_host = {};
        std::string m_path = "/";
        std::string m_port = {};
        std::string m_query = {};
        std::string m_fragment = {};
    };

    /** Tuple-like equality of all observable members */
    auto operator==(URL const& a, URL const& b) -> bool;
    auto operator!=(URL const& a, URL const& b) -> bool;

    /** A functional equivalent to ``URL::append_path``. */
    auto operator/(URL const& url, std::string_view subpath) -> URL;
    auto operator/(URL&& url, std::string_view subpath) -> URL;
}

template <>
struct std::hash<mamba::util::URL>
{
    auto operator()(const mamba::util::URL& p) const -> std::size_t;
};
#endif
