// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_URL_HPP
#define MAMBA_CORE_URL_HPP

#include <powerloader/url.hpp>

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
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
    using powerloader::decode_url;
    using powerloader::encode_url;
    using powerloader::has_scheme;
    using powerloader::is_path;
    using powerloader::path_to_url;
    using powerloader::unc_url;

    void split_anaconda_token(const std::string& url, std::string& cleaned_url, std::string& token);

    void split_scheme_auth_token(const std::string& url,
                                 std::string& remaining_url,
                                 std::string& scheme,
                                 std::string& auth,
                                 std::string& token);

    bool compare_cleaned_url(const std::string& url1, const std::string& url2);

    template <class S, class... Args>
    std::string join_url(const S& s, const Args&... args);

    // Only returns a cache name without extension
    std::string cache_name_from_url(const std::string& url);
    std::string hide_secrets(const std::string_view& str);

    using powerloader::URLHandler;

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
