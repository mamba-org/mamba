// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <spdlog/spdlog.h>

#include "curl.hpp"

namespace mamba
{
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
}  // namespace mamba
