// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_URL_HPP
#define MAMBA_CORE_URL_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>


namespace mamba
{
    std::string concat_scheme_url(const std::string& scheme, const std::string& location);

    std::string build_url(
        const std::optional<std::string>& auth,
        const std::string& scheme,
        const std::string& base,
        bool with_credential
    );

    void split_platform(
        const std::vector<std::string>& known_platforms,
        const std::string& url,
        const std::string& context_platform,
        std::string& cleaned_url,
        std::string& platform
    );

    /**
     * If @p url starts with a scheme, return it, otherwise return empty string.
     *
     * Does not include "://"
     */
    [[nodiscard]] auto url_get_scheme(std::string_view url) -> std::string_view;

    /**
     * Retrun true if @p url starts with a URL scheme.
     */
    [[nodiscard]] auto url_has_scheme(std::string_view url) -> bool;

    /**
     * Check if a Windows path (not URL) starts with a drive letter.
     */
    [[nodiscard]] auto path_has_drive_letter(std::string_view path) -> bool;

    void split_anaconda_token(const std::string& url, std::string& cleaned_url, std::string& token);

    void split_scheme_auth_token(
        const std::string& url,
        std::string& remaining_url,
        std::string& scheme,
        std::string& auth,
        std::string& token
    );

    bool compare_cleaned_url(const std::string& url1, const std::string& url2);

    bool is_path(const std::string& input);
    std::string path_to_url(const std::string& path);

    template <class S, class... Args>
    std::string join_url(const S& s, const Args&... args);

    /**
     * Convert UNC2 file URI to UNC4.
     *
     * Windows paths can be expressed in a form, called UNC, where it is possible to express a
     * server location, as in "\\hostname\folder\data.xml".
     * This can be succefully encoded in a file URI like "file://hostname/folder/data.xml"
     * since file URI contain a part for the hostname (empty hostname file URI must start with
     * "file:///").
     * Since CURL does not support hostname in file URI, we can encode UNC hostname as part
     * of the path (called 4-slash), where it becomes "file:////hostname/folder/data.xml".
     *
     * This function leaves all non-matching URI (inluding a number of invalid URI for unkown
     * legacy reasons taken from ``url_to_path`` in conda.common.path) unchanged.
     *
     * @see https://learn.microsoft.com/en-us/dotnet/standard/io/file-path-formats#unc-paths
     * @see https://en.wikipedia.org/wiki/File_URI_scheme
     */
    std::string file_uri_unc2_to_unc4(std::string_view url);

    std::string encode_url(const std::string& url);
    std::string decode_url(const std::string& url);
    // Only returns a cache name without extension
    std::string cache_name_from_url(const std::string& url);

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

    namespace detail
    {
        inline std::string join_url_impl(std::string& s)
        {
            return s;
        }

        template <class S, class... Args>
        inline std::string join_url_impl(std::string& s1, const S& s2, const Args&... args)
        {
            if (!s2.empty())
            {
                if (s1.empty() || s1.back() != '/')
                {
                    s1 += '/';
                }
                s1 += s2;
            }
            return join_url_impl(s1, args...);
        }

        template <class... Args>
        inline std::string join_url_impl(std::string& s1, const char* s2, const Args&... args)
        {
            if (s1.size() && s1.back() != '/')
            {
                s1 += '/';
            }
            s1 += s2;
            return join_url_impl(s1, args...);
        }
    }  // namespace detail

    inline std::string join_url()
    {
        return "";
    }

    template <class S, class... Args>
    inline std::string join_url(const S& s, const Args&... args)
    {
        std::string res = s;
        return detail::join_url_impl(res, args...);
    }
}  // namespace mamba

#endif
