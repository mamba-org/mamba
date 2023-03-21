// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CURL_HPP
#define MAMBA_CURL_HPP

// #include <map>
// #include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

extern "C"
{
#include <curl/curl.h>
}

#include <fmt/core.h>
#include <tl/expected.hpp>

namespace mamba
{
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

    class CURLHandle
    {
    public:

        //         using end_callback_type = std::function<CbReturnCode(const Response&)>;
        //         explicit CURLHandle(const Context& ctx);
        //         CURLHandle(const Context& ctx, const std::string& url);
        CURLHandle();
        CURLHandle(CURLHandle&& rhs);
        CURLHandle& operator=(CURLHandle&& rhs);
        ~CURLHandle();

        const std::pair<std::string_view, CurlLogLevel> init_curl_ssl_session();

        //         CURLHandle& url(const std::string& url, const proxy_map_type& proxies);
        //         CURLHandle& accept_encoding();
        //         CURLHandle& user_agent(const std::string& user_agent);
        //
        //         Response perform();
        //         void finalize_transfer();

        template <class T>
        tl::expected<T, CURLcode> get_info(CURLINFO option);

        // This is made public because it is used internally in quite some files
        CURL* handle();

        CURLHandle& add_header(const std::string& header);
        CURLHandle& add_headers(const std::vector<std::string>& headers);
        CURLHandle& reset_headers();

        template <class T>
        CURLHandle& set_opt(CURLoption opt, const T& val);

        CURLHandle& set_opt_header();

        // TODO Make this private after more wrapping...
        char m_errorbuffer[CURL_ERROR_SIZE];
        //         void set_default_callbacks();
        //         CURLHandle& set_end_callback(end_callback_type func);
        //
        //         CURLHandle& upload(std::ifstream& stream);
        //         CURLHandle& upload(std::istringstream& stream);

    private:

        //         void init_handle(const Context& ctx);
        //         void finalize_transfer(Response& response);

        CURL* m_handle;
        curl_slist* p_headers = nullptr;

        //         std::unique_ptr<Response> response;
        //         end_callback_type end_callback;
    };

    // TODO: restrict the possible implementations in the cpp file
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
