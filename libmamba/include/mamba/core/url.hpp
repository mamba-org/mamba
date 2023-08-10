// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_URL_HPP
#define MAMBA_CORE_URL_HPP

extern "C"
{
#include <curl/urlapi.h>
}

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// typedef enum {
//   CURLUE_OK,
//   CURLUE_BAD_HANDLE,          /* 1 */
//   CURLUE_BAD_PARTPOINTER,     /* 2 */
//   CURLUE_MALFORMED_INPUT,     /* 3 */
//   CURLUE_BAD_PORT_NUMBER,     /* 4 */
//   CURLUE_UNSUPPORTED_SCHEME,  /* 5 */
//   CURLUE_URLDECODE,           /* 6 */
//   CURLUE_OUT_OF_MEMORY,       /* 7 */
//   CURLUE_USER_NOT_ALLOWED,    /* 8 */
//   CURLUE_UNKNOWN_PART,        /* 9 */
//   CURLUE_NO_SCHEME,           /* 10 */
//   CURLUE_NO_USER,             /* 11 */
//   CURLUE_NO_PASSWORD,         /* 12 */
//   CURLUE_NO_OPTIONS,          /* 13 */
//   CURLUE_NO_HOST,             /* 14 */
//   CURLUE_NO_PORT,             /* 15 */
//   CURLUE_NO_QUERY,            /* 16 */
//   CURLUE_NO_FRAGMENT          /* 17 */
// } CURLUcode;

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

    bool has_scheme(const std::string& url);

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

    class URLHandler
    {
    public:

        URLHandler(const std::string& url = "");
        ~URLHandler();

        URLHandler(const URLHandler&);
        URLHandler& operator=(const URLHandler&);

        URLHandler(URLHandler&&);
        URLHandler& operator=(URLHandler&&);

        std::string url(bool strip_scheme = false);

        std::string scheme() const;
        std::string host() const;
        std::string path() const;
        std::string port() const;

        std::string query() const;
        std::string fragment() const;
        std::string options() const;

        std::string auth() const;
        std::string user() const;
        std::string password() const;
        std::string zoneid() const;

        URLHandler& set_scheme(const std::string& scheme);
        URLHandler& set_host(const std::string& host);
        URLHandler& set_path(const std::string& path);
        URLHandler& set_port(const std::string& port);

        URLHandler& set_query(const std::string& query);
        URLHandler& set_fragment(const std::string& fragment);
        URLHandler& set_options(const std::string& options);

        URLHandler& set_user(const std::string& user);
        URLHandler& set_password(const std::string& password);
        URLHandler& set_zoneid(const std::string& zoneid);

    private:

        std::string get_part(CURLUPart part) const;
        void set_part(CURLUPart part, const std::string& s);

        std::string m_url;
        CURLU* m_handle;
        bool m_has_scheme;
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
