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
}

#include <fmt/core.h>
#include <tl/expected.hpp>

namespace mamba
{
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
    }

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

    class CURLId
    {
    public:

        bool operator==(const CURLId& rhs) const;
        bool operator!=(const CURLId& rhs) const;
        bool operator<(const CURLId& rhs) const;
        bool operator<=(const CURLId& rhs) const;
        bool operator>(const CURLId& rhs) const;
        bool operator>=(const CURLId& rhs) const;

        std::size_t hash() const noexcept;

    private:

        explicit CURLId(CURL* handle = nullptr);

        CURL* p_handle;

        friend class CURLHandle;
        friend class CURLMultiHandle;
    };
}

template <>
struct std::hash<mamba::CURLId>
{
    std::size_t operator()(const mamba::CURLId& arg) const noexcept
    {
        return arg.hash();
    }
};

namespace mamba
{
    class CURLHandle
    {
    public:

        CURLHandle();
        CURLHandle(CURLHandle&& rhs);
        CURLHandle& operator=(CURLHandle&& rhs);
        ~CURLHandle();

        const std::pair<std::string_view, CurlLogLevel> get_ssl_backend_info();

        template <class T>
        tl::expected<T, CURLcode> get_info(CURLINFO option) const;

        void configure_handle(
            const std::string& url,
            const bool set_low_speed_opt,
            const long connect_timeout_secs,
            const bool set_ssl_no_revoke,
            const std::optional<std::string>& proxy,
            const std::string& ssl_verify
        );

        void reset_handle();

        CURLHandle& add_header(const std::string& header);
        CURLHandle& add_headers(const std::vector<std::string>& headers);
        CURLHandle& reset_headers();

        template <class T>
        CURLHandle& set_opt(CURLoption opt, const T& val);

        CURLHandle& set_opt_header();

        const char* get_error_buffer() const;
        std::string get_curl_effective_url() const;

        [[deprecated]] std::size_t get_result() const;
        [[deprecated]] bool is_curl_res_ok() const;

        [[deprecated]] void set_result(CURLcode res);

        [[deprecated]] std::string get_res_error() const;

        // Side-effect programming, to remove
        [[deprecated]] bool can_proceed();
        void perform();

        CURLId get_id() const;

        // New API to avoid storing result
        static bool is_curl_res_ok(CURLcode res);
        static std::string get_res_error(CURLcode res);
        static bool can_retry(CURLcode res);

    private:

        CURL* m_handle;
        CURLcode m_result;  // Enum range from 0 to 99
        curl_slist* p_headers = nullptr;
        char m_errorbuffer[CURL_ERROR_SIZE];

        friend CURL* unwrap(const CURLHandle&);
    };

    bool operator==(const CURLHandle& lhs, const CURLHandle& rhs);
    bool operator!=(const CURLHandle& lhs, const CURLHandle& rhs);

    struct CURLMultiResponse
    {
        CURLId m_handle_id;
        CURLcode m_transfer_result;
        bool m_transfer_done;
    };

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

}  // namespace mamba

#endif  // MAMBA_CURL_HPP
