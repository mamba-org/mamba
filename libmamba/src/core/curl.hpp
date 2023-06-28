// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CURL_HPP
#define MAMBA_CURL_HPP

#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

// TODO to be removed later and forward declare specific curl structs
extern "C"
{
#include <curl/curl.h>
#include <curl/urlapi.h>
}

#include <fmt/core.h>
#include <tl/expected.hpp>

namespace mamba
{
    std::string unc_url(const std::string& url);

    namespace curl
    {
        void configure_curl_handle(
            CURL* handle,
            const std::string& url,
            const bool set_low_speed_opt,
            const long connect_timeout_secs,
            const bool set_ssl_no_revoke,
            const std::optional<std::string>& proxy,
            const std::string& ssl_verify
        );

        bool check_resource_exists(
            const std::string& url,
            const bool set_low_speed_opt,
            const long connect_timeout_secs,
            const bool set_ssl_no_revoke,
            const std::optional<std::string>& proxy,
            const std::string& ssl_verify
        );

        std::string encode_url(const std::string& url);
        std::string decode_url(const std::string& url);
    }  // namespace curl

    enum class CurlLogLevel
    {
        kInfo,
        kWarning,
        kError
    };

    class curl_error : public std::runtime_error
    {
    public:

        curl_error(const std::string& what = "", bool serious = false);
        bool is_serious() const;

    private:

        bool m_serious;
    };

    /****************
     *  CURLHandle  *
     ****************/

    class CURLHandle
    {
    public:

        CURLHandle();
        CURLHandle(CURLHandle&& rhs);
        CURLHandle& operator=(CURLHandle&& rhs);
        ~CURLHandle();

        const std::pair<std::string_view, CurlLogLevel> get_ssl_backend_info();

        template <class T>
        tl::expected<T, CURLcode> get_info(CURLINFO option);

        void configure_handle(
            const std::string& url,
            const bool set_low_speed_opt,
            const long connect_timeout_secs,
            const bool set_ssl_no_revoke,
            const std::optional<std::string>& proxy,
            const std::string& ssl_verify
        );

        CURLHandle& add_header(const std::string& header);
        CURLHandle& add_headers(const std::vector<std::string>& headers);
        CURLHandle& reset_headers();

        template <class T>
        CURLHandle& set_opt(CURLoption opt, const T& val);

        CURLHandle& set_opt_header();

        const char* get_error_buffer() const;
        std::string get_curl_effective_url();

        std::size_t get_result() const;
        bool is_curl_res_ok() const;

        void set_result(CURLcode res);

        std::string get_res_error() const;

        bool can_proceed();
        void perform();

    private:

        CURL* m_handle;
        CURLcode m_result;  // Enum range from 0 to 99
        curl_slist* p_headers = nullptr;
        char m_errorbuffer[CURL_ERROR_SIZE];

        friend CURL* unwrap(const CURLHandle&);
    };

    bool operator==(const CURLHandle& lhs, const CURLHandle& rhs);
    bool operator!=(const CURLHandle& lhs, const CURLHandle& rhs);

    template <class T>
    CURLHandle& CURLHandle::set_opt(CURLoption opt, const T& val)
    {
        CURLcode ok;
        if constexpr (std::is_same<T, std::string>())
        {
            ok = curl_easy_setopt(m_handle, opt, val.c_str());
        }
        else if constexpr (std::is_same<T, bool>())
        {
            ok = curl_easy_setopt(m_handle, opt, val ? 1L : 0L);
        }
        else
        {
            ok = curl_easy_setopt(m_handle, opt, val);
        }
        if (ok != CURLE_OK)
        {
            throw curl_error(fmt::format("curl: curl_easy_setopt failed {}", curl_easy_strerror(ok)));
        }
        return *this;
    }

    /********************
     *   CURLReference  *
     ********************/

    class CURLReference
    {
    public:

        CURLReference(CURL* handle);

    private:

        CURL* p_handle;

        friend CURL* unwrap(const CURLReference&);
    };

    bool operator==(const CURLHandle& lhs, const CURLReference& rhs);
    bool operator==(const CURLReference& lhs, const CURLHandle& rhs);
    bool operator==(const CURLReference& lhs, const CURLReference& rhs);
    bool operator!=(const CURLHandle& lhs, const CURLReference& rhs);
    bool operator!=(const CURLReference& lhs, const CURLHandle& rhs);
    bool operator!=(const CURLReference& lhs, const CURLReference& rhs);

    /************************
     *   CURLMultiResponse  *
     ************************/

    struct CURLMultiResponse
    {
        CURLReference m_handle_ref;
        CURLcode m_transfer_result;
        bool m_transfer_done;
    };

    /**********************
     *   CURLMultiHandle  *
     **********************/

    class CURLMultiHandle
    {
    public:

        using response_type = std::optional<CURLMultiResponse>;

        explicit CURLMultiHandle(std::size_t max_parallel_downloads);
        ~CURLMultiHandle();

        CURLMultiHandle(CURLMultiHandle&&);
        CURLMultiHandle& operator=(CURLMultiHandle&&);

        void add_handle(const CURLHandle&);
        void remove_handle(const CURLHandle&);

        std::size_t perform();
        response_type pop_message();
        std::size_t get_timeout(std::size_t max_timeout = 1000u) const;
        std::size_t wait(std::size_t timeout);

    private:

        CURLM* p_handle;
        std::size_t m_max_parallel_downloads = 5;
    };

    /******************
     *   CURLUHandle  *
     ******************/

    // NOTE:
    // This is used in set_curl_url
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

    class CURLUHandle
    {
    public:

        CURLUHandle();
        ~CURLUHandle();

        CURLUHandle(CURLUHandle&&);
        CURLUHandle& operator=(CURLUHandle&&);

        void set_curl_url(const std::string& url, bool has_scheme);

        std::string get_part(const std::string& part, bool has_scheme) const;
        void set_part(const std::string& part, const std::string& s, const std::string& url);

    private:

        CURLU* m_handle;
    };

}  // namespace mamba

#endif  // MAMBA_CURL_HPP
