// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

// TODO remove all these includes later?
#include <spdlog/spdlog.h>

#include "mamba/core/mamba_fs.hpp"  // for fs::exists
#include "mamba/core/util.hpp"      // for hide_secrets

#include "curl.hpp"

namespace mamba
{
    namespace curl
    {
        void configure_curl_handle(
            CURL* handle,
            const std::string& url,
            const bool set_low_speed_opt,
            const long connect_timeout_secs,
            const bool ssl_no_revoke,
            const std::optional<std::string>& proxy,
            const std::string& ssl_verify
        )
        {
            curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
            curl_easy_setopt(handle, CURLOPT_NETRC, CURL_NETRC_OPTIONAL);
            curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

            // This can improve throughput significantly, see
            // https://github.com/curl/curl/issues/9601
            curl_easy_setopt(handle, CURLOPT_BUFFERSIZE, 100 * 1024);

            // DO NOT SET TIMEOUT as it will also take into account multi-start time and
            // it's just wrong curl_easy_setopt(m_handle, CURLOPT_TIMEOUT,
            // Context::instance().remote_fetch_info.read_timeout_secs);

            // TODO while libcurl in conda now _has_ http2 support we need to fix mamba to
            // work properly with it this includes:
            // - setting the cache stuff correctly
            // - fixing how the progress bar works
            curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

            if (set_low_speed_opt)
            {
                curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, 60L);
                curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, 30L);
            }

            curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, connect_timeout_secs);

            if (ssl_no_revoke)
            {
                curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NO_REVOKE);
            }

            if (proxy)
            {
                curl_easy_setopt(handle, CURLOPT_PROXY, proxy->c_str());
                // TODO LOG_INFO was used here instead; to be modified later following the new log
                // procedure (TBD)
                spdlog::info("Using Proxy {}", hide_secrets(*proxy));
            }

            if (ssl_verify.size())
            {
                if (ssl_verify == "<false>")
                {
                    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
                    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);
                    if (proxy)
                    {
                        curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
                        curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
                    }
                }
                else if (ssl_verify == "<system>")
                {
#ifdef LIBMAMBA_STATIC_DEPS
                    curl_easy_setopt(handle, CURLOPT_CAINFO, nullptr);
                    if (proxy)
                    {
                        curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO, nullptr);
                    }
#endif
                }
                else
                {
                    if (!fs::exists(ssl_verify))
                    {
                        throw std::runtime_error("ssl_verify does not contain a valid file path.");
                    }
                    else
                    {
                        curl_easy_setopt(handle, CURLOPT_CAINFO, ssl_verify.c_str());
                        if (proxy)
                        {
                            curl_easy_setopt(handle, CURLOPT_PROXY_CAINFO, ssl_verify.c_str());
                        }
                    }
                }
            }
        }
    }

    /**************
     * curl_error *
     **************/

    curl_error::curl_error(const std::string& what, bool serious)
        : std::runtime_error(what)
        , m_serious(serious)
    {
    }

    bool curl_error::is_serious() const
    {
        return m_serious;
    }

    /**************
     * CURLHandle *
     **************/
    CURLHandle::CURLHandle()  //(const Context& ctx)
        : m_handle(curl_easy_init())
    {
        if (m_handle == nullptr)
        {
            throw curl_error("Could not initialize CURL handle");
        }

        // Set error buffer
        m_errorbuffer[0] = '\0';
        set_opt(CURLOPT_ERRORBUFFER, m_errorbuffer);
    }

    CURLHandle::CURLHandle(CURLHandle&& rhs)
        : m_handle(std::move(rhs.m_handle))
        , p_headers(std::move(rhs.p_headers))
    {
        std::swap(m_errorbuffer, rhs.m_errorbuffer);
    }

    CURLHandle& CURLHandle::operator=(CURLHandle&& rhs)
    {
        using std::swap;
        swap(m_handle, rhs.m_handle);
        swap(p_headers, rhs.p_headers);
        swap(m_errorbuffer, rhs.m_errorbuffer);
        return *this;
    }

    CURLHandle::~CURLHandle()
    {
        curl_easy_cleanup(m_handle);
        curl_slist_free_all(p_headers);
    }

    // TODO Rework this after a logging solution is established in the mamba project
    const std::pair<std::string_view, CurlLogLevel> CURLHandle::get_ssl_backend_info()
    {
        std::pair<std::string_view, CurlLogLevel> log;
        const struct curl_tlssessioninfo* info = NULL;
        CURLcode res = curl_easy_getinfo(m_handle, CURLINFO_TLS_SSL_PTR, &info);
        if (info && !res)
        {
            if (info->backend == CURLSSLBACKEND_OPENSSL)
            {
                log = std::make_pair(std::string_view("Using OpenSSL backend"), CurlLogLevel::kInfo);
            }
            else if (info->backend == CURLSSLBACKEND_SECURETRANSPORT)
            {
                log = std::make_pair(
                    std::string_view("Using macOS SecureTransport backend"),
                    CurlLogLevel::kInfo
                );
            }
            else if (info->backend == CURLSSLBACKEND_SCHANNEL)
            {
                log = std::make_pair(
                    std::string_view("Using Windows Schannel backend"),
                    CurlLogLevel::kInfo
                );
            }
            else if (info->backend != CURLSSLBACKEND_NONE)
            {
                log = std::make_pair(
                    std::string_view("Using an unknown (to mamba) SSL backend"),
                    CurlLogLevel::kInfo
                );
            }
            else if (info->backend == CURLSSLBACKEND_NONE)
            {
                log = std::make_pair(
                    std::string_view(
                        "No SSL backend found! Please check how your cURL library is configured."
                    ),
                    CurlLogLevel::kWarning
                );
            }
        }
        return log;
    }

    template <class T>
    tl::expected<T, CURLcode> CURLHandle::get_info(CURLINFO option)
    {
        T val;
        CURLcode result = curl_easy_getinfo(m_handle, option, &val);
        if (result != CURLE_OK)
        {
            return tl::unexpected(result);
        }
        return val;
    }

    // WARNING curl_easy_getinfo MUST have its third argument pointing to long, char*, curl_slist*
    // or double
    template tl::expected<long, CURLcode> CURLHandle::get_info(CURLINFO option);
    template tl::expected<char*, CURLcode> CURLHandle::get_info(CURLINFO option);
    template tl::expected<double, CURLcode> CURLHandle::get_info(CURLINFO option);
    template tl::expected<curl_slist*, CURLcode> CURLHandle::get_info(CURLINFO option);

    template <class I>
    tl::expected<I, CURLcode> CURLHandle::get_integer_info(CURLINFO option)
    {
        auto res = get_info<long>(option);
        if (res)
        {
            return static_cast<I>(res.value());
        }
        else
        {
            return tl::unexpected(res.error());
        }
    }

    template <>
    tl::expected<std::size_t, CURLcode> CURLHandle::get_info(CURLINFO option)
    {
        return get_integer_info<std::size_t>(option);
    }

    template <>
    tl::expected<int, CURLcode> CURLHandle::get_info(CURLINFO option)
    {
        return get_integer_info<int>(option);
    }

    template <>
    tl::expected<std::string, CURLcode> CURLHandle::get_info(CURLINFO option)
    {
        auto res = get_info<char*>(option);
        if (res)
        {
            return std::string(res.value());
        }
        else
        {
            return tl::unexpected(res.error());
        }
    }

    // TODO to be removed from the API
    CURL* CURLHandle::handle()
    {
        return m_handle;
    }

    CURLHandle& CURLHandle::add_header(const std::string& header)
    {
        p_headers = curl_slist_append(p_headers, header.c_str());
        if (!p_headers)
        {
            throw std::bad_alloc();
        }
        return *this;
    }

    CURLHandle& CURLHandle::add_headers(const std::vector<std::string>& headers)
    {
        for (auto& h : headers)
        {
            add_header(h);
        }
        return *this;
    }

    CURLHandle& CURLHandle::reset_headers()
    {
        curl_slist_free_all(p_headers);
        p_headers = nullptr;
        return *this;
    }

    CURLHandle& CURLHandle::set_opt_header()
    {
        set_opt(CURLOPT_HTTPHEADER, p_headers);
        return *this;
    }

    const char* CURLHandle::get_error_buffer() const
    {
        return m_errorbuffer;
    }

}  // namespace mamba
