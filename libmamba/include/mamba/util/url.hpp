// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_URL_HPP
#define MAMBA_UTIL_URL_HPP

#include <string>
#include <string_view>


namespace mamba::util
{

    [[nodiscard]] std::string encode_url(const std::string& url);
    [[nodiscard]] std::string decode_url(const std::string& url);

    /**
     * Class representing a URL.
     *
     * All URL have a non-empty scheme, host, and path.
     */
    class URL
    {
    public:

        enum class StripScheme : bool
        {
            no,
            yes,
        };

        inline static constexpr std::string_view https = "https";
        inline static constexpr std::string_view localhost = "localhost";

        [[nodiscard]] static auto parse(std::string_view url) -> URL;

        /** Create a local URL. */
        URL() = default;

        /** Return the scheme, always non-empty. */
        [[nodiscard]] auto scheme() const -> const std::string&;

        /** Set a non-empty scheme. */
        URL& set_scheme(std::string_view scheme);

        /** Return the user, or empty if none. */
        [[nodiscard]] auto user() const -> const std::string&;

        /** Set or clear the user. */
        URL& set_user(std::string_view user);

        /** Return the password, or empty if none. */
        [[nodiscard]] auto password() const -> const std::string&;

        /** Set or clear the password. */
        URL& set_password(std::string_view password);

        /** Return the basic authetification string. */
        [[nodiscard]] auto authentication() const -> std::string;

        /** Return the host, always non-empty. */
        [[nodiscard]] auto host() const -> const std::string&;

        /** Set a non-empty host. */
        URL& set_host(std::string_view host);

        /** Return the port, or empty if none. */
        [[nodiscard]] auto port() const -> const std::string&;

        /** Set or clear the port. */
        URL& set_port(std::string_view port);

        /** Return the autority part of the URL. */
        [[nodiscard]] auto authority() const -> std::string;

        /** Return the path, always starts with a '/'. */
        [[nodiscard]] auto path() const -> const std::string&;

        /**
         * Return the path.
         *
         * For a "file" scheme, with a Windows path containing a drive, the leading '/' is
         * stripped.
         */
        [[nodiscard]] auto pretty_path() const -> std::string_view;

        /** Set the path, a leading '/' is added if abscent. */
        URL& set_path(std::string_view path);

        /** Return the query, or empty if none. */
        [[nodiscard]] auto query() const -> const std::string&;

        /** Set or clear the query. */
        URL& set_query(std::string_view query);

        /** Return the fragment, or empty if none. */
        [[nodiscard]] auto fragment() const -> const std::string&;

        /** Set or clear the fragment. */
        URL& set_fragment(std::string_view fragment);

        /**
         * Return the full url.
         *
         * @param strip If true, remove the scheme and "localhost" on file URI.
         */
        [[nodiscard]] auto str(StripScheme strip = StripScheme::no) const -> std::string;

    private:

        std::string m_scheme = std::string(https);
        std::string m_user = {};
        std::string m_password = {};
        std::string m_host = std::string(localhost);
        std::string m_path = "/";
        std::string m_port = {};
        std::string m_query = {};
        std::string m_fragment = {};
    };
}
#endif
