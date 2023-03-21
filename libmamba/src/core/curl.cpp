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

        //         init_handle(ctx);

        // Set error buffer
        m_errorbuffer[0] = '\0';
        set_opt(CURLOPT_ERRORBUFFER, m_errorbuffer);
    }

    CURLHandle::CURLHandle(CURLHandle&& rhs)
        : m_handle(std::move(rhs.m_handle))
        , p_headers(std::move(rhs.p_headers))
    //         , response(std::move(rhs.response))
    //         , end_callback(std::move(rhs.end_callback))
    {
        std::copy(&rhs.m_errorbuffer[0], &rhs.m_errorbuffer[CURL_ERROR_SIZE], &m_errorbuffer[0]);
    }

    CURLHandle& CURLHandle::operator=(CURLHandle&& rhs)
    {
        using std::swap;
        swap(m_handle, rhs.m_handle);
        swap(p_headers, rhs.p_headers);
        swap(m_errorbuffer, rhs.m_errorbuffer);
        //         swap(response, rhs.response);
        //         swap(end_callback, rhs.end_callback);
        return *this;
    }

    CURLHandle::~CURLHandle()
    {
        if (m_handle)
        {
            curl_easy_cleanup(m_handle);
        }
        if (p_headers)
        {
            curl_slist_free_all(p_headers);
        }
    }

    const std::pair<std::string_view, CurlLogLevel> CURLHandle::init_curl_ssl_session()
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
    //     void CURLHandle::init_handle(const Context& ctx)
    //     {
    //         set_opt(CURLOPT_FOLLOWLOCATION, 1);
    //         set_opt(CURLOPT_NETRC, CURL_NETRC_OPTIONAL);
    //         set_opt(CURLOPT_MAXREDIRS, 6);
    //         set_opt(CURLOPT_CONNECTTIMEOUT, ctx.connect_timeout);
    //         set_opt(CURLOPT_LOW_SPEED_TIME, ctx.low_speed_time);
    //         set_opt(CURLOPT_LOW_SPEED_LIMIT, ctx.low_speed_limit);
    //         set_opt(CURLOPT_BUFFERSIZE, ctx.transfer_buffersize);
    //
    //         if (ctx.disable_ssl)
    //         {
    //             spdlog::warn("SSL verification is disabled");
    //             set_opt(CURLOPT_SSL_VERIFYHOST, 0);
    //             set_opt(CURLOPT_SSL_VERIFYPEER, 0);
    //
    //             // also disable proxy SSL verification
    //             set_opt(CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
    //             set_opt(CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
    //         }
    //         else
    //         {
    //             spdlog::warn("SSL verification is ENABLED");
    //
    //             set_opt(CURLOPT_SSL_VERIFYHOST, 2);
    //             set_opt(CURLOPT_SSL_VERIFYPEER, 1);
    //
    //             // Windows SSL backend doesn't support this
    //             CURLcode verifystatus = curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYSTATUS, 0);
    //             if (verifystatus != CURLE_OK && verifystatus != CURLE_NOT_BUILT_IN)
    //                 throw curl_error("Could not initialize CURL handle");
    //
    //             if (ctx.ssl_no_default_ca_info)
    //             {
    //                 set_opt<char*>(CURLOPT_CAINFO, nullptr);
    //             }
    //
    //             if (!ctx.ssl_ca_info.empty())
    //             {
    //                 set_opt(CURLOPT_CAINFO, ctx.ssl_ca_info.c_str());
    //             }
    //
    //             if (ctx.ssl_no_revoke)
    //             {
    //                 set_opt(CURLOPT_SSL_OPTIONS, ctx.ssl_no_revoke);
    //             }
    //         }
    //
    //         set_opt(CURLOPT_FTP_USE_EPSV, (long) ctx.ftp_use_seepsv);
    //         set_opt(CURLOPT_FILETIME, (long) ctx.preserve_filetime);
    //
    //         if (ctx.verbosity > 0)
    //             set_opt(CURLOPT_VERBOSE, (long) 1L);
    //     }

    //     CURLHandle::CURLHandle(const Context& ctx, const std::string& url)
    //         : CURLHandle(ctx)
    //     {
    //         this->url(url, ctx.proxy_map);
    //     }


    //     CURLHandle& CURLHandle::url(const std::string& url, const proxy_map_type& proxies)
    //     {
    //         set_opt(CURLOPT_URL, url.c_str());
    //         const auto match = proxy_match(proxies, url);
    //         if (match)
    //         {
    //             set_opt(CURLOPT_PROXY, match.value().c_str());
    //         }
    //         else
    //         {
    //             set_opt(CURLOPT_PROXY, nullptr);
    //         }
    //         return *this;
    //     }

    //     CURLHandle& CURLHandle::accept_encoding()
    //     {
    //         set_opt(CURLOPT_ACCEPT_ENCODING, "");
    //         return *this;
    //     }

    //     CURLHandle& CURLHandle::user_agent(const std::string& user_agent)
    //     {
    //         add_header(fmt::format("User-Agent: {} {}", user_agent, curl_version()));
    //         return *this;
    //     }

    //     Response CURLHandle::perform()
    //     {
    //         set_default_callbacks();
    //         CURLcode curl_result = curl_easy_perform(handle());
    //         if (curl_result != CURLE_OK)
    //         {
    //             throw curl_error(fmt::format("{} [{}]", curl_easy_strerror(curl_result),
    //             m_errorbuffer));
    //         }
    //         finalize_transfer(*response);
    //         return std::move(*response.release());
    //     }
    //
    //     void CURLHandle::finalize_transfer()
    //     {
    //         if (response)
    //             finalize_transfer(*response);
    //     }
    //
    //     void CURLHandle::finalize_transfer(Response& lresponse)
    //     {
    //         lresponse.fill_values(*this);
    //         if (!lresponse.ok())
    //         {
    //             spdlog::error("Received {}: {}", lresponse.http_status,
    //             lresponse.content.value());
    //         }
    //         if (end_callback)
    //         {
    //             end_callback(lresponse);
    //         }
    //     }

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

    template <>
    tl::expected<std::size_t, CURLcode> CURLHandle::get_info(CURLINFO option)
    {
        auto res = get_info<long>(option);
        if (res)
        {
            return static_cast<std::size_t>(res.value());
        }
        else
        {
            return tl::unexpected(res.error());
        }
    }

    template <>
    tl::expected<int, CURLcode> CURLHandle::get_info(CURLINFO option)
    {
        auto res = get_info<long>(option);
        if (res)
        {
            return static_cast<int>(res.value());
        }
        else
        {
            return tl::unexpected(res.error());
        }
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

    //     namespace
    //     {
    //         template <class T>
    //         std::size_t ostream_callback(char* buffer, std::size_t size, std::size_t nitems, T*
    //         stream)
    //         {
    //             stream->write(buffer, size * nitems);
    //             return size * nitems;
    //         }
    //
    //         template <class T>
    //         std::size_t string_callback(char* buffer, std::size_t size, std::size_t nitems, T*
    //         string)
    //         {
    //             string->append(buffer, size * nitems);
    //             return size * nitems;
    //         }
    //
    //         template <class T>
    //         std::size_t header_map_callback(char* buffer,
    //                                         std::size_t size,
    //                                         std::size_t nitems,
    //                                         T* header_map)
    //         {
    //             auto kv = parse_header(std::string_view(buffer, size * nitems));
    //             if (!kv.first.empty())
    //             {
    //                 (*header_map)[kv.first] = kv.second;
    //             }
    //             return size * nitems;
    //         }
    //     }

    //     void CURLHandle::set_default_callbacks()
    //     {
    //         response.reset(new Response);
    //         // check if there is something set already for these values
    //         set_opt(CURLOPT_HEADERFUNCTION, header_map_callback<std::map<std::string,
    //         std::string>>); set_opt(CURLOPT_HEADERDATA, &response->headers);
    //
    //         set_opt(CURLOPT_WRITEFUNCTION, string_callback<std::string>);
    //         response->content = std::string();
    //         set_opt(CURLOPT_WRITEDATA, &response->content.value());
    //     }

    //     CURLHandle& CURLHandle::set_end_callback(end_callback_type func)
    //     {
    //         end_callback = std::move(func);
    //         return *this;
    //     }

    //     namespace
    //     {
    //         template <class T>
    //         std::size_t read_callback(char* ptr, std::size_t size, std::size_t nmemb, T* stream)
    //         {
    //             // copy as much data as possible into the 'ptr' buffer, but no more than
    //             // 'size' * 'nmemb' bytes!
    //             stream->read(ptr, size * nmemb);
    //             return stream->gcount();
    //         }
    //
    //         template <class S>
    //         CURLHandle& upload_impl(CURLHandle& curl, S& stream)
    //         {
    //             stream.seekg(0, stream.end);
    //             curl_off_t fsize = stream.tellg();
    //             stream.seekg(0, stream.beg);
    //
    //             if (fsize != -1)
    //             {
    //                 curl.set_opt(CURLOPT_INFILESIZE_LARGE, fsize);
    //             }
    //
    //             curl.set_opt(CURLOPT_UPLOAD, 1L);
    //             curl.set_opt(CURLOPT_READFUNCTION, read_callback<S>);
    //             curl.set_opt(CURLOPT_READDATA, &stream);
    //             return curl;
    //         }
    //     }
    //
    //
    //     CURLHandle& CURLHandle::upload(std::ifstream& stream)
    //     {
    //         return upload_impl(*this, stream);
    //     }
    //
    //     CURLHandle& CURLHandle::upload(std::istringstream& stream)
    //     {
    //         return upload_impl(*this, stream);
    //     }

}  // namespace mamba
