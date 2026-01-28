// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/core/invoke.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/iterator.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url.hpp"
#include "mamba/util/url_manip.hpp"

#include "curl.hpp"
#include "downloader_impl.hpp"

namespace mamba::download
{
    namespace
    {

        constexpr std::array<const char*, 10> cert_locations{
            "/etc/ssl/certs/ca-certificates.crt",                 // Debian/Ubuntu/Gentoo etc.
            "/etc/pki/tls/certs/ca-bundle.crt",                   // Fedora/RHEL 6
            "/etc/ssl/ca-bundle.pem",                             // OpenSUSE
            "/etc/pki/tls/cacert.pem",                            // OpenELEC
            "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",  // CentOS/RHEL 7
            "/etc/ssl/cert.pem",                                  // Alpine Linux
            // MacOS
            "/System/Library/OpenSSL/certs/cert.pem",
            "/usr/local/etc/openssl/cert.pem",
            "/usr/local/share/certs/ca-root-nss.crt",
            "/usr/local/share/certs/ca-root.crt",
        };

        void init_remote_fetch_params(RemoteFetchParams& remote_fetch_params)
        {
            if (!remote_fetch_params.curl_initialized)
            {
                if (remote_fetch_params.ssl_verify == "<false>")
                {
                    LOG_DEBUG << "'ssl_verify' not activated, skipping cURL SSL init";
                    remote_fetch_params.curl_initialized = true;
                    return;
                }
#ifdef LIBMAMBA_STATIC_DEPS
                CURLHandle handle;
                auto init_res = handle.get_ssl_backend_info();
                switch (init_res.second)
                {
                    case CurlLogLevel::kInfo:
                    {
                        LOG_INFO << init_res.first;
                        break;
                    }
                    case CurlLogLevel::kWarning:
                    {
                        LOG_WARNING << init_res.first;
                        break;
                    }
                    case CurlLogLevel::kError:
                    {
                        LOG_ERROR << init_res.first;
                        break;
                    }
                }
#endif

                if (!remote_fetch_params.ssl_verify.size())
                {
                    if (auto ca = util::get_env("REQUESTS_CA_BUNDLE"); ca.has_value())
                    {
                        remote_fetch_params.ssl_verify = ca.value();
                        LOG_INFO << "Using REQUESTS_CA_BUNDLE " << remote_fetch_params.ssl_verify;
                    }
                }
                // TODO: Adapt the semantic of `<system>` to decouple the use of CA certificates
                // from `conda-forge::ca-certificates` and the system CA certificates.
                else if (remote_fetch_params.ssl_verify == "<system>")
                {
                    // See the location of the CA certificates as distributed by
                    // `conda-forge::ca-certificates`:
                    // https://github.com/conda-forge/ca-certificates-feedstock/blob/main/recipe/meta.yaml#L25-L29
                    const fs::u8path prefix_relative_conda_cert_path = (util::on_win ? fs::u8path("Library") / "ssl" / "cacert.pem" : fs::u8path("ssl") / "cert.pem");

                    const fs::u8path executable_path = get_self_exe_path();

                    // Find the environment prefix using the path of the `mamba` or `micromamba`
                    // executable (we cannot assume the existence of an environment variable or
                    // etc.).
                    //
                    // `mamba` or `micromamba` is installed at:
                    //
                    //    - `${PREFIX}/bin/{mamba,micromamba}` on Unix
                    //    - `${PREFIX}/Library/bin/{mamba,micromamba}.exe` on Windows
                    //
                    const fs::u8path env_prefix
                        = (util::on_win ? executable_path.parent_path().parent_path().parent_path()
                                        : executable_path.parent_path().parent_path());

                    const fs::u8path env_prefix_conda_cert = env_prefix
                                                             / prefix_relative_conda_cert_path;

                    LOG_INFO << "Checking for CA certificates in the same prefix as the executable installation: "
                             << env_prefix_conda_cert;

                    if (fs::exists(env_prefix_conda_cert))
                    {
                        LOG_INFO << "Using CA certificates from `conda-forge::ca-certificates` installed in the same prefix "
                                 << "as the executable installation (i.e " << env_prefix_conda_cert
                                 << ")";
                        remote_fetch_params.ssl_verify = env_prefix_conda_cert;
                        remote_fetch_params.curl_initialized = true;
                        return;
                    }

                    // Try to use the CA certificates from `conda-forge::ca-certificates` installed
                    // in the root prefix.
                    const fs::u8path root_prefix = detail::get_root_prefix();
                    const fs::u8path root_prefix_conda_cert = root_prefix
                                                              / prefix_relative_conda_cert_path;

                    LOG_INFO << "Checking for CA certificates at the root prefix: "
                             << root_prefix_conda_cert;

                    if (fs::exists(root_prefix_conda_cert))
                    {
                        LOG_INFO << "Using CA certificates from `conda-forge::ca-certificates` installed in the root prefix "
                                 << "(i.e " << root_prefix_conda_cert << ")";
                        remote_fetch_params.ssl_verify = root_prefix_conda_cert;
                        remote_fetch_params.curl_initialized = true;
                        return;
                    }

                    // Fallback on system CA certificates.
                    bool found = false;

                    // TODO: find if one needs to specify a CA certificate on Windows or not
                    // given that the location of system's CA certificates is not clear on Windows.
                    // For now, just use `libcurl` and the SSL libraries' default.
                    if (util::on_win)
                    {
                        LOG_INFO << "Using libcurl/the SSL library's default CA certification";
                        remote_fetch_params.ssl_verify = "";
                        found = true;
                        remote_fetch_params.curl_initialized = true;
                        return;
                    }

                    for (const auto& loc : cert_locations)
                    {
                        if (fs::exists(loc))
                        {
                            LOG_INFO << "Using system CA certificates at: " << loc;
                            remote_fetch_params.ssl_verify = loc;
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        const std::string msg = "No CA certificates found on system, aborting";
                        LOG_ERROR << msg;
                        throw mamba_error(msg, mamba_error_code::openssl_failed);
                    }
                }

                remote_fetch_params.curl_initialized = true;
            }
        }

        struct EnvRemoteParams
        {
            bool set_low_speed_opt = false;
            bool set_ssl_no_revoke = false;
        };

        EnvRemoteParams get_env_remote_params(const RemoteFetchParams& params)
        {
            // TODO: we should probably store set_low_speed_limit and set_ssl_no_revoke in
            // RemoteFetchParams if the request is slower than 30b/s for 60 seconds, cancel.
            const std::string no_low_speed_limit = util::get_env("MAMBA_NO_LOW_SPEED_LIMIT")
                                                       .value_or("0");
            const bool set_low_speed_opt = (no_low_speed_limit == "0");

            const std::string ssl_no_revoke_env = util::get_env("MAMBA_SSL_NO_REVOKE").value_or("0");
            const bool set_ssl_no_revoke = params.ssl_no_revoke || (ssl_no_revoke_env != "0");

            return { set_low_speed_opt, set_ssl_no_revoke };
        }
    }

    /**********************************
     * DownloadAttempt implementation *
     **********************************/

    struct DownloadAttempt::Impl
    {
        Impl(
            CURLHandle& handle,
            const MirrorRequest& request,
            CURLMultiHandle& downloader,
            const RemoteFetchParams& params,
            const specs::AuthenticationDataBase& auth_info,
            bool verbose,
            on_success_callback on_success,
            on_failure_callback on_error,
            on_stop_callback on_stop
        );

        bool finish_download(CURLMultiHandle& downloader, CURLcode code);
        void clean_attempt(CURLMultiHandle& downloader, bool erase_downloaded);
        void invoke_progress_callback(const Event&) const;

        void configure_handle(
            const RemoteFetchParams& params,
            const specs::AuthenticationDataBase& auth_info,
            bool verbose
        );
        void configure_handle_headers(
            const RemoteFetchParams& params,
            const specs::AuthenticationDataBase& auth_info
        );

        size_t write_data(char* buffer, size_t data);

        static size_t curl_header_callback(char* buffer, size_t size, size_t nbitems, void* self);
        static size_t curl_write_callback(char* buffer, size_t size, size_t nbitems, void* self);
        static int curl_progress_callback(
            void* f,
            curl_off_t total_to_download,
            curl_off_t now_downloaded,
            curl_off_t,
            curl_off_t
        );

        bool can_retry(CURLcode code) const;
        bool can_retry(const TransferData& data) const;

        TransferData get_transfer_data() const;
        Error build_download_error(CURLcode code) const;
        Error build_download_error(TransferData data) const;
        Success build_download_success(TransferData data) const;

        CURLHandle* p_handle = nullptr;
        const MirrorRequest* p_request = nullptr;
        std::atomic_bool m_is_stop_requested = false;
        on_success_callback m_success_callback;
        on_failure_callback m_failure_callback;
        on_stop_callback m_stop_callback;
        std::size_t m_retry_wait_seconds = std::size_t(0);
        std::unique_ptr<CompressionStream> p_stream = nullptr;
        std::ofstream m_file;
        mutable std::string m_response = "";
        std::string m_cache_control;
        std::string m_etag;
        std::string m_last_modified;
    };

    DownloadAttempt::DownloadAttempt(
        CURLHandle& handle,
        const MirrorRequest& request,
        CURLMultiHandle& downloader,
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info,
        bool verbose,
        on_success_callback on_success,
        on_failure_callback on_error,
        on_stop_callback on_stop
    )
        : p_impl(
              std::make_unique<Impl>(
                  handle,
                  request,
                  downloader,
                  params,
                  auth_info,
                  verbose,
                  std::move(on_success),
                  std::move(on_error),
                  std::move(on_stop)
              )
          )
    {
    }

    auto DownloadAttempt::create_completion_function() -> completion_function
    {
        return [impl = p_impl.get()](CURLMultiHandle& handle, CURLcode code)
        { return impl->finish_download(handle, code); };
    }

    auto DownloadAttempt::request_stop() -> void
    {
        if (p_impl)
        {
            p_impl->m_is_stop_requested = true;
        }
    }

    DownloadAttempt::Impl::Impl(
        CURLHandle& handle,
        const MirrorRequest& request,
        CURLMultiHandle& downloader,
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info,
        bool verbose,
        on_success_callback on_success,
        on_failure_callback on_error,
        on_stop_callback on_stop
    )
        : p_handle(&handle)
        , p_request(&request)
        , m_success_callback(std::move(on_success))
        , m_failure_callback(std::move(on_error))
        , m_stop_callback(std::move(on_stop))
        , m_retry_wait_seconds(static_cast<std::size_t>(params.retry_timeout))
    {
        p_stream = make_compression_stream(
            p_request->url,
            p_request->is_repodata_zst,
            [this](char* in, std::size_t size) { return this->write_data(in, size); }
        );
        configure_handle(params, auth_info, verbose);
        downloader.add_handle(*p_handle);
    }

    namespace
    {
        bool is_http_status_ok(int http_status)
        {
            // Note: http_status == 0 for files
            return http_status / 100 == 2 || http_status == 304 || http_status == 0;
        }
    }

    bool DownloadAttempt::Impl::finish_download(CURLMultiHandle& downloader, CURLcode code)
    {
        if (code == CURLE_ABORTED_BY_CALLBACK)
        {
            clean_attempt(downloader, true);
            return m_stop_callback();
        }

        if (!CURLHandle::is_curl_res_ok(code))
        {
            Error error = build_download_error(code);
            clean_attempt(downloader, true);
            invoke_progress_callback(error);
            return m_failure_callback(std::move(error));
        }
        else
        {
            TransferData data = get_transfer_data();
            if (!is_http_status_ok(data.http_status))
            {
                Error error = build_download_error(std::move(data));
                clean_attempt(downloader, true);
                invoke_progress_callback(error);
                return m_failure_callback(std::move(error));
            }
            else
            {
                Success success = build_download_success(std::move(data));
                clean_attempt(downloader, false);
                invoke_progress_callback(success);
                return m_success_callback(std::move(success));
            }
        }
    }

    void DownloadAttempt::Impl::clean_attempt(CURLMultiHandle& downloader, bool erase_downloaded)
    {
        downloader.remove_handle(*p_handle);
        p_handle->reset_handle();

        if (m_file.is_open())
        {
            m_file.close();
        }
        if (erase_downloaded && p_request->filename.has_value()
            && fs::exists(p_request->filename.value()))
        {
            fs::remove(p_request->filename.value());
        }

        m_response.clear();
        m_cache_control.clear();
        m_etag.clear();
        m_last_modified.clear();
    }

    void DownloadAttempt::Impl::invoke_progress_callback(const Event& event) const
    {
        if (p_request->progress.has_value())
        {
            p_request->progress.value()(event);
        }
    }

    namespace
    {
        int
        curl_debug_callback(CURL* /* handle */, curl_infotype type, char* data, size_t size, void*)
        {
            static constexpr auto symbol_for = [](curl_infotype type_)
            {
                switch (type_)
                {
                    case CURLINFO_TEXT:
                        return "*";
                    case CURLINFO_HEADER_OUT:
                        return ">";
                    case CURLINFO_HEADER_IN:
                        return "<";
                    default:
                        return "";
                };
            };

            switch (type)
            {
                case CURLINFO_TEXT:
                case CURLINFO_HEADER_OUT:
                case CURLINFO_HEADER_IN:
                {
                    auto message = fmt::format(
                        "{} {}",
                        symbol_for(type),
                        Console::hide_secrets(std::string_view(data, size))
                    );
                    logging::log(
                        { .message = std::move(message),
                          .level = log_level::info,
                          .source = log_source::libcurl }
                    );
                    break;
                }
                default:
                    // WARNING Using `hide_secrets` here will give a seg fault on linux,
                    // and other errors on other platforms
                    break;
            }
            return 0;
        }

        std::string
        build_transfer_message(int http_status, const std::string& effective_url, std::size_t size)
        {
            std::stringstream ss;
            ss << "Transfer finalized, status: " << http_status << " [" << effective_url << "] "
               << size << " bytes";
            return ss.str();
        }
    }

    void DownloadAttempt::Impl::configure_handle(
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info,
        bool verbose
    )
    {
        const auto [set_low_speed_opt, set_ssl_no_revoke] = get_env_remote_params(params);

        p_handle->configure_handle(
            util::file_uri_unc2_to_unc4(p_request->url),
            set_low_speed_opt,
            params.connect_timeout_secs,
            set_ssl_no_revoke,
            proxy_match(p_request->url, params.proxy_servers),
            params.ssl_verify
        );

        if (!p_request->username.empty())
        {
            p_handle->set_opt(CURLOPT_USERNAME, p_request->username);
        }

        if (!p_request->password.empty())
        {
            p_handle->set_opt(CURLOPT_PASSWORD, p_request->password);
        }

        p_handle->set_opt(CURLOPT_NOBODY, p_request->check_only);

        p_handle->set_opt(CURLOPT_HEADERFUNCTION, &DownloadAttempt::Impl::curl_header_callback);
        p_handle->set_opt(CURLOPT_HEADERDATA, this);

        p_handle->set_opt(CURLOPT_WRITEFUNCTION, &DownloadAttempt::Impl::curl_write_callback);
        p_handle->set_opt(CURLOPT_WRITEDATA, this);

        if (p_request->progress.has_value())
        {
            p_handle->set_opt(CURLOPT_XFERINFOFUNCTION, &DownloadAttempt::Impl::curl_progress_callback);
            p_handle->set_opt(CURLOPT_XFERINFODATA, this);
            p_handle->set_opt(CURLOPT_NOPROGRESS, 0L);
        }

        if (util::ends_with(p_request->url, ".json"))
        {
            // accept all encodings supported by the libcurl build
            p_handle->set_opt(CURLOPT_ACCEPT_ENCODING, "");
            p_handle->add_header("Content-Type: application/json");
        }

        p_handle->set_opt(CURLOPT_VERBOSE, verbose);

        configure_handle_headers(params, auth_info);
        p_handle->set_opt(CURLOPT_DEBUGFUNCTION, curl_debug_callback);
    }

    void DownloadAttempt::Impl::configure_handle_headers(
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info
    )
    {
        p_handle->reset_headers();

        std::string user_agent = fmt::format("User-Agent: {} {}", params.user_agent, curl_version());
        p_handle->add_header(user_agent);

        // get url host
        const auto url_handler = util::URL::parse(p_request->url).value();
        auto host = url_handler.host();
        const auto port = url_handler.port();
        if (port.size())
        {
            host += ":" + port;
        }

        // TODO How should this be handled if not empty?
        // (think about precedence with request token auth header added below)
        if (auto it = auth_info.find_weaken(host); it != auth_info.end())
        {
            if (const auto& auth = it->second; std::holds_alternative<specs::BearerToken>(auth))
            {
                p_handle->add_header(
                    fmt::format("Authorization: Bearer {}", std::get<specs::BearerToken>(auth).token)
                );
            }
        }

        if (p_request->etag.has_value())
        {
            p_handle->add_header("If-None-Match:" + p_request->etag.value());
        }

        if (p_request->last_modified.has_value())
        {
            p_handle->add_header("If-Modified-Since:" + p_request->last_modified.value());
        }

        // Add specific request headers
        // (token auth header, and application type when getting the manifest)
        if (!p_request->headers.empty())
        {
            p_handle->add_headers(p_request->headers);
        }

        p_handle->set_opt_header();
    }

    size_t DownloadAttempt::Impl::write_data(char* buffer, size_t size)
    {
        if (p_request->filename.has_value())
        {
            if (!m_file.is_open())
            {
                m_file = open_ofstream(p_request->filename.value(), std::ios::binary);
                if (!m_file)
                {
                    LOG_ERROR << "Could not open file for download " << p_request->filename.value()
                              << ": " << strerror(errno);
                    // Return a size _different_ than the expected write size to signal an error
                    return size + 1;
                }
            }

            m_file.write(buffer, static_cast<std::streamsize>(size));

            if (!m_file)
            {
                LOG_ERROR << "Could not write to file " << p_request->filename.value() << ": "
                          << strerror(errno);
                // Return a size _different_ than the expected write size to signal an error
                return size + 1;
            }
        }
        else
        {
            m_response.append(buffer, size);
        }
        return size;
    }

    size_t
    DownloadAttempt::Impl::curl_header_callback(char* buffer, size_t size, size_t nbitems, void* self)
    {
        auto* s = reinterpret_cast<DownloadAttempt::Impl*>(self);

        const size_t buffer_size = size * nbitems;
        const std::string_view header(buffer, buffer_size);
        auto colon_idx = header.find(':');
        if (colon_idx != std::string_view::npos)
        {
            std::string_view key = header.substr(0, colon_idx);
            colon_idx++;
            // remove spaces
            while (std::isspace(header[colon_idx]))
            {
                ++colon_idx;
            }

            // remove \r\n header ending
            const auto header_end = header.find_first_of("\r\n");
            std::string_view value = header.substr(
                colon_idx,
                (header_end > colon_idx) ? header_end - colon_idx : 0
            );

            // http headers are case insensitive!
            const std::string lkey = util::to_lower(key);
            if (lkey == "etag")
            {
                s->m_etag = value;
            }
            else if (lkey == "cache-control")
            {
                s->m_cache_control = value;
            }
            else if (lkey == "last-modified")
            {
                s->m_last_modified = value;
            }
        }

        return buffer_size;
    }

    size_t
    DownloadAttempt::Impl::curl_write_callback(char* buffer, size_t size, size_t nbitems, void* self)
    {
        return reinterpret_cast<DownloadAttempt::Impl*>(self)->p_stream->write(buffer, size * nbitems);
    }

    int DownloadAttempt::Impl::curl_progress_callback(
        void* f,
        curl_off_t total_to_download,
        curl_off_t now_downloaded,
        curl_off_t,
        curl_off_t
    )
    {
        auto* self = reinterpret_cast<DownloadAttempt::Impl*>(f);

        if (self->m_is_stop_requested)
        {
            // Stop has been requested, we need to abort the download.
            // Returning `1` will end make libcurl abort and return `CURLE_ABORTED_BY_CALLBACK`.
            // see https://curl.se/libcurl/c/CURLOPT_XFERINFOFUNCTION.html for details.
            return 1;
        }

        const auto speed_Bps = self->p_handle->get_info<std::size_t>(CURLINFO_SPEED_DOWNLOAD_T)
                                   .value_or(0);
        const size_t total = total_to_download ? static_cast<std::size_t>(total_to_download)
                                               : self->p_request->expected_size.value_or(0);
        self->p_request->progress.value()(
            Progress{ static_cast<std::size_t>(now_downloaded), total, speed_Bps }
        );
        return 0;
    }

    namespace http
    {
        static constexpr int PAYLOAD_TOO_LARGE = 413;
        static constexpr int TOO_MANY_REQUESTS = 429;
        static constexpr int INTERNAL_SERVER_ERROR = 500;
        static constexpr int ARBITRARY_ERROR = 10000;
    }

    bool DownloadAttempt::Impl::can_retry(CURLcode code) const
    {
        return p_handle->can_retry(code) && !util::starts_with(p_request->url, "file://");
    }

    bool DownloadAttempt::Impl::can_retry(const TransferData& data) const
    {
        return (data.http_status == http::PAYLOAD_TOO_LARGE
                || data.http_status == http::TOO_MANY_REQUESTS
                || data.http_status >= http::INTERNAL_SERVER_ERROR)
               && !util::starts_with(p_request->url, "file://");
    }

    TransferData DownloadAttempt::Impl::get_transfer_data() const
    {
        // Curl transforms file URI like file:///C/something into file://C/something, which
        // may lead to wrong comparisons later. When the URL is a file URI, we know there is
        // no redirection and we can use the input URL as the effective URL.
        std::string url = util::is_file_uri(p_request->url)
                              ? p_request->url
                              : p_handle->get_info<char*>(CURLINFO_EFFECTIVE_URL).value();
        return {
            /* .http_status = */ p_handle->get_info<int>(CURLINFO_RESPONSE_CODE)
                .value_or(http::ARBITRARY_ERROR),
            /* .effective_url = */ std::move(url),
            /* .dwonloaded_size = */ p_handle->get_info<std::size_t>(CURLINFO_SIZE_DOWNLOAD_T).value_or(0),
            /* .average_speed = */ p_handle->get_info<std::size_t>(CURLINFO_SPEED_DOWNLOAD_T).value_or(0)
        };
    }

    Error DownloadAttempt::Impl::build_download_error(CURLcode code) const
    {
        Error error;
        std::stringstream strerr;
        strerr << "Download error (" << code << ") " << p_handle->get_res_error(code) << " ["
               << p_handle->get_curl_effective_url() << "]\n"
               << p_handle->get_error_buffer();
        error.message = strerr.str();

        if (can_retry(code))
        {
            error.retry_wait_seconds = m_retry_wait_seconds;
        }
        return error;
    }

    Error DownloadAttempt::Impl::build_download_error(TransferData data) const
    {
        Error error;
        if (can_retry(data))
        {
            error.retry_wait_seconds = p_handle->get_info<std::size_t>(CURLINFO_RETRY_AFTER)
                                           .value_or(m_retry_wait_seconds);
        }
        error.message = build_transfer_message(data.http_status, data.effective_url, data.downloaded_size);
        error.transfer = std::move(data);
        return error;
    }

    Success DownloadAttempt::Impl::build_download_success(TransferData data) const
    {
        Content content;
        if (p_request->filename.has_value())
        {
            content = Filename{ p_request->filename.value() };
        }
        else
        {
            content = Buffer{ std::move(m_response) };
        }

        return { /*.content = */ std::move(content),
                 /*.transfer = */ std::move(data),
                 /*.cache_control = */ m_cache_control,
                 /*.etag = */ m_etag,
                 /*.last_modified = */ m_last_modified };
    }

    /********************************
     * MirrorAttempt implementation *
     ********************************/

    /*
     * MirrorAttempt FSM:
     * WAITING_SEQUENCE_START:
     *     - prepare_attempt => PREPARING_DOWNLOAD
     * PREPARING_DOWNLOAD:
     *     - set_transfer_started => RUNNING_DOWNLOAD
     * RUNNING_DOWNLOAD:
     *     - set_stopped() => SEQUENCE_STOPPED
     *     - set_state(true) => LAST_REQUEST_FINISHED
     *     - set_state(false) => LAST_REQUEST_FAILED
     *     - set_state(Error with wait_next_retry) => LAST_REQUEST_FAILED
     *     - set_state(Error no wait_next_retry  ) => SEQUENCE_FAILED
     * LAST_REQUEST_FINISHED:
     *     - m_step == m_request_generators.size() ? => SEQUENCE_FINISHED
     * LAST_REQUEST_FAILED:
     *     - m_retries == p_mirror->max_retries ? => SEQUENCE_FAILED
     */
    MirrorAttempt::MirrorAttempt(Mirror& mirror, const std::string& url_path, const std::string& spec_sha256)
        : p_mirror(&mirror)
        , m_request_generators(p_mirror->get_request_generators(url_path, spec_sha256))
    {
    }

    expected_t<void> MirrorAttempt::invoke_on_success(const Success& res) const
    {
        if (m_request.value().on_success.has_value())
        {
            auto ret = safe_invoke(m_request.value().on_success.value(), res);
            return ret.has_value() ? ret.value() : forward_error(ret);
        }
        return expected_t<void>();
    }

    void MirrorAttempt::invoke_on_failure(const Error& res) const
    {
        if (m_request.value().on_failure.has_value())
        {
            // We dont want to propagate errors coming from user's callbacks
            [[maybe_unused]] auto result = safe_invoke(m_request.value().on_failure.value(), res);
        }
    }

    void MirrorAttempt::invoke_on_stopped() const
    {
        if (m_request.value().on_stopped.has_value())
        {
            // We dont want to propagate errors coming from user's callbacks
            [[maybe_unused]] auto result = safe_invoke(m_request.value().on_stopped.value());
        }
    }

    void MirrorAttempt::prepare_request(const Request& initial_request)
    {
        if (m_state != State::LAST_REQUEST_FAILED)
        {
            m_request = m_request_generators[m_step](initial_request, p_last_content);
            ++m_step;
        }
        else
        {
            m_next_retry = std::nullopt;
            ++m_retries;
            LOG_DEBUG << "Last request failed! Tried " << m_retries << " over "
                      << p_mirror->max_retries() << " times";
        }
    }

    auto MirrorAttempt::prepare_attempt(
        CURLHandle& handle,
        CURLMultiHandle& downloader,
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info,
        bool verbose,
        on_success_callback on_success,
        on_failure_callback on_error,
        on_stop_callback on_stop
    ) -> completion_function
    {
        LOG_DEBUG << "Preparing download...";
        m_state = State::PREPARING_DOWNLOAD;
        m_attempt = DownloadAttempt(
            handle,
            m_request.value(),
            downloader,
            params,
            auth_info,
            verbose,
            std::move(on_success),
            std::move(on_error),
            std::move(on_stop)
        );
        return m_attempt.create_completion_function();
    }

    bool MirrorAttempt::can_start_transfer() const
    {
        return m_state == State::WAITING_SEQUENCE_START || m_state == State::LAST_REQUEST_FINISHED
               || (m_state == State::LAST_REQUEST_FAILED && can_retry());
    }

    bool MirrorAttempt::has_failed() const
    {
        return m_state == State::SEQUENCE_FAILED;
    }

    bool MirrorAttempt::has_finished() const
    {
        auto res = (m_state == State::SEQUENCE_FINISHED) || (m_step == m_request_generators.size());
        return res;
    }

    bool MirrorAttempt::has_stopped() const
    {
        return m_state == State::SEQUENCE_STOPPED;
    }

    void MirrorAttempt::set_transfer_started()
    {
        m_state = State::RUNNING_DOWNLOAD;
        p_mirror->increase_running_transfers();
    }

    void MirrorAttempt::set_state(bool success)
    {
        if (success)
        {
            if (m_step == m_request_generators.size())
            {
                m_state = State::SEQUENCE_FINISHED;
            }
            else
            {
                m_state = State::LAST_REQUEST_FINISHED;
            }
            update_transfers_done(true);
        }
        else
        {
            if (m_retries < p_mirror->max_retries())
            {
                m_state = State::LAST_REQUEST_FAILED;
            }
            else
            {
                m_state = State::SEQUENCE_FAILED;
            }
            update_transfers_done(false);
        }
    }

    void MirrorAttempt::set_state(const Error& res)
    {
        if (res.retry_wait_seconds.has_value() && m_retries < p_mirror->max_retries())
        {
            m_state = State::LAST_REQUEST_FAILED;
            m_next_retry = std::chrono::steady_clock::now()
                           + std::chrono::seconds(res.retry_wait_seconds.value());
        }
        else
        {
            m_state = State::SEQUENCE_FAILED;
        }
        update_transfers_done(false);
    }

    void MirrorAttempt::set_stopped()
    {
        // TODO: NOT COMPLETE
        m_state = State::SEQUENCE_STOPPED;
    }

    void MirrorAttempt::update_last_content(const Content* content)
    {
        p_last_content = content;
    }

    bool MirrorAttempt::can_retry() const
    {
        return !m_next_retry.has_value() || m_next_retry.value() < std::chrono::steady_clock::now();
    }

    void MirrorAttempt::update_transfers_done(bool success)
    {
        p_mirror->update_transfers_done(success, !m_request.value().check_only);
    }

    void MirrorAttempt::request_stop()
    {
        // TODO: CHECK WHAT ELSE TO DO HERE
        m_attempt.request_stop();
    }

    /**********************************
     * DownloadTracker implementation *
     **********************************/

    DownloadTracker::DownloadTracker(
        const Request& request,
        const mirror_set_view& mirrors,
        DownloadTrackerOptions options
    )
        : m_handle()
        , p_initial_request(&request)
        , m_mirror_set(mirrors)
        , m_options(std::move(options))
        , m_state(State::WAITING)
        , m_attempt_results()
        , m_tried_mirrors()
        , m_mirror_attempt()
    {
        prepare_mirror_attempt();
        if (has_failed())
        {
            Error error;
            error.message = std::string("Could not find mirrors for channel ")
                            + hide_secrets(p_initial_request->mirror_name);
            m_attempt_results.push_back(tl::unexpected(std::move(error)));
        }
    }

    auto DownloadTracker::prepare_new_attempt(
        CURLMultiHandle& handle,
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info,
        bool verbose

    ) -> completion_map_entry
    {
        m_state = State::PREPARING;

        m_mirror_attempt.prepare_request(*p_initial_request);
        auto completion_func = m_mirror_attempt.prepare_attempt(
            m_handle,
            handle,
            params,
            auth_info,
            verbose,
            [this](Success res)
            {
                expected_t<void> finalize_res = invoke_on_success(res);
                set_state(finalize_res.has_value());
                throw_if_required(finalize_res);
                save(std::move(res));
                return is_waiting();
            },
            [this](Error res)
            {
                invoke_on_failure(res);
                set_state(res);
                throw_if_required(res);
                save(std::move(res));
                return is_waiting();
            },
            [this]
            {
                complete_as_stopped();
                return false;
            }
        );
        return { m_handle.get_id(), completion_func };
    }

    bool DownloadTracker::has_failed() const
    {
        return m_state == State::FAILED;
    }

    bool DownloadTracker::can_start_transfer() const
    {
        return is_waiting() && (m_mirror_attempt.can_start_transfer() || can_try_other_mirror());
    }

    void DownloadTracker::set_transfer_started()
    {
        m_state = State::RUNNING;
        m_mirror_attempt.set_transfer_started();
    }

    const Result& DownloadTracker::get_result() const
    {
        assert(not m_attempt_results.empty());
        return m_attempt_results.back();
    }

    expected_t<void> DownloadTracker::invoke_on_success(const Success& res) const
    {
        if (!m_mirror_attempt.has_finished())
        {
            return m_mirror_attempt.invoke_on_success(res);
        }
        else
        {
            if (p_initial_request->on_success.has_value())
            {
                auto ret = safe_invoke(p_initial_request->on_success.value(), res);
                return ret.has_value() ? ret.value() : forward_error(ret);
            }
        }
        return expected_t<void>();
    }

    void DownloadTracker::invoke_on_failure(const Error& res) const
    {
        if (!m_mirror_attempt.has_finished())
        {
            m_mirror_attempt.invoke_on_failure(res);
        }
        else
        {
            if (p_initial_request->on_failure.has_value())
            {
                // We dont want to propagate errors coming from user's callbacks
                [[maybe_unused]] auto result = safe_invoke(p_initial_request->on_failure.value(), res);
            }
        }
    }

    void DownloadTracker::invoke_on_stopped() const
    {
        if (p_initial_request->on_stopped.has_value())
        {
            // We dont want to propagate errors coming from user's callbacks
            [[maybe_unused]] auto result = safe_invoke(p_initial_request->on_stopped.value());
        }
    }

    bool DownloadTracker::is_waiting() const
    {
        return m_state == State::WAITING;
    }

    bool DownloadTracker::is_done() const
    {
        return m_state == State::FAILED or m_state == State::STOPPED or m_state == State::FINISHED;
    }

    bool DownloadTracker::is_ongoing() const
    {
        return not is_waiting() and not is_done();
    }

    bool DownloadTracker::can_try_other_mirror() const
    {
        bool is_file = util::starts_with(p_initial_request->url_path, "file://");
        bool is_check = p_initial_request->check_only;
        return !is_file && !is_check && m_tried_mirrors.size() < m_options.max_mirror_tries;
    }

    void DownloadTracker::set_state(bool success)
    {
        m_mirror_attempt.set_state(success);
        if (success)
        {
            m_state = m_mirror_attempt.has_finished() ? State::FINISHED : State::WAITING;
        }
        else
        {
            set_error_state();
        }
    }

    void DownloadTracker::set_state(const Error& res)
    {
        m_mirror_attempt.set_state(res);
        set_error_state();
    }

    void DownloadTracker::set_error_state()
    {
        if (!m_mirror_attempt.has_failed() || can_try_other_mirror())
        {
            m_state = State::WAITING;
            if (m_mirror_attempt.has_failed())
            {
                prepare_mirror_attempt();
            }
        }
        else
        {
            m_state = State::FAILED;
        }
    }

    void DownloadTracker::set_stopped()
    {
        // TODO: more?
        m_mirror_attempt.set_stopped();
        m_state = State::STOPPED;
    }

    void DownloadTracker::request_stop()
    {
        // TODO: ?
        m_mirror_attempt.request_stop();
    }

    void DownloadTracker::throw_if_required(const expected_t<void>& res)
    {
        if (m_state == State::FAILED && !p_initial_request->ignore_failure && m_options.fail_fast)
        {
            throw res.error();
        }
    }

    void DownloadTracker::throw_if_required(const Error& res)
    {
        if (m_state == State::FAILED && !p_initial_request->ignore_failure)
        {
            throw std::runtime_error(res.message);
        }
    }

    void DownloadTracker::save(Success&& res)
    {
        res.attempt_number = m_attempt_results.size() + std::size_t(1);
        m_attempt_results.push_back(Result(std::move(res)));
        m_mirror_attempt.update_last_content(&(get_result().value().content));
    }

    void DownloadTracker::save(Error&& res)
    {
        res.attempt_number = m_attempt_results.size() + std::size_t(1);
        m_attempt_results.push_back(tl::unexpected(std::move(res)));
    }

    void DownloadTracker::prepare_mirror_attempt()
    {
        Mirror* mirror = select_new_mirror();
        if (mirror != nullptr)
        {
            m_tried_mirrors.insert(mirror->id());
            m_mirror_attempt = MirrorAttempt(
                *mirror,
                p_initial_request->url_path,
                p_initial_request->sha256
            );
        }
        else
        {
            m_state = State::FAILED;
        }
    }

    namespace
    {
        template <class F>
        Mirror* find_mirror(const mirror_set_view& mirrors, F&& f)
        {
            auto iter = std::find_if(mirrors.begin(), mirrors.end(), std::forward<F>(f));
            Mirror* mirror = (iter == mirrors.end()) ? nullptr : iter->get();
            return mirror;
        }
    }

    Mirror* DownloadTracker::select_new_mirror() const
    {
        Mirror* new_mirror = find_mirror(
            m_mirror_set,
            [this](const auto& mirror)
            {
                return !has_tried_mirror(mirror.get()) && !is_bad_mirror(mirror.get())
                       && mirror->can_accept_more_connections();
            }
        );

        std::size_t iteration = 0;
        while (new_mirror == nullptr && ++iteration < m_options.max_mirror_tries)
        {
            new_mirror = find_mirror(
                m_mirror_set,
                [iteration](const auto& mirror)
                {
                    return iteration > mirror->capture_stats().failed_transfers
                           && mirror->can_accept_more_connections();
                }
            );
        }
        return new_mirror;
    }

    bool DownloadTracker::has_tried_mirror(Mirror* mirror) const
    {
        return m_tried_mirrors.contains(mirror->id());
    }

    bool DownloadTracker::is_bad_mirror(Mirror* mirror) const
    {
        const auto stats = mirror->capture_stats();
        return stats.successful_transfers == 0 && stats.failed_transfers >= mirror->max_retries();
    }

    void DownloadTracker::complete_as_stopped()
    {
        invoke_on_stopped();
        set_stopped();
        save(make_stop_error());
    }

    /*****************************
     * DOWNLOADER IMPLEMENTATION *
     *****************************/

    Downloader::Downloader(
        MultiRequest requests,
        const mirror_map& mirrors,
        Options options,
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info
    )
        : m_requests(std::move(requests))
        , m_trackers()
        , m_curl_handle(options.download_threads)
        , m_options(std::move(options))
        , p_mirrors(&mirrors)
        , p_params(&params)
        , p_auth_info(&auth_info)
    {
        if (m_options.sort)
        {
            std::sort(
                m_requests.begin(),
                m_requests.end(),
                [](const Request& a, const Request& b) -> bool
                { return a.expected_size.value_or(SIZE_MAX) > b.expected_size.value_or(SIZE_MAX); }
            );
        }

        m_trackers.reserve(m_requests.size());
        std::size_t max_retries = static_cast<std::size_t>(params.max_retries);
        DownloadTrackerOptions tracker_options{ max_retries, options.fail_fast };
        std::transform(
            m_requests.begin(),
            m_requests.end(),
            std::back_inserter(m_trackers),
            [tracker_options, this](const Request& req)
            {
                return DownloadTracker(req, p_mirrors->get_mirrors(req.mirror_name), tracker_options);
            }
        );
        m_waiting_count = m_trackers.size();
        auto failed_count = std::count_if(
            m_trackers.begin(),
            m_trackers.end(),
            [](const auto& tracker) { return tracker.has_failed(); }
        );
        m_waiting_count -= static_cast<size_t>(failed_count);
    }

    void Downloader::request_stop()
    {
        for (auto& tracker : m_trackers)
        {
            if (tracker.is_ongoing())
            {
                // FIXME: this hack is because of the console output overwriting the cli output
                // lines: log 2 lines per stopped download so that at least one get displayed to the
                // user.
                LOG_WARNING << "!!!!";
                LOG_WARNING << "Interruption requested by user - stopping download... ";
                tracker.request_stop();
            }
        }
        logging::flush_logs();

        // Waiting downloads need to be stopped at this point to avoid waiting for never finishing
        // downloads (because they never started), even if the stopping was requested before all
        // downloads. The downloads that already started will end naturally when receiving the
        // proper libcurl message.
        force_stop_waiting_downloads();
    }

    MultiResult Downloader::download()
    {
        bool was_interrupted = false;
        while (!download_done())
        {
            if (is_sig_interrupted() && not was_interrupted)
            {
                was_interrupted = true;
                request_stop();
                download_while_stopping();
                break;
            }
            prepare_next_downloads();
            update_downloads();
        }

        return build_result();
    }

    void Downloader::download_while_stopping()
    {
        while (!download_done())
        {
            update_downloads();
        }
        invoke_unexpected_termination();
    }

    void Downloader::force_stop_waiting_downloads()
    {
        for (auto& tracker : m_trackers)
        {
            if (tracker.is_waiting())
            {
                tracker.complete_as_stopped();
                assert(m_waiting_count > 0);
                --m_waiting_count;
            }
        }
    }

    void Downloader::prepare_next_downloads()
    {
        size_t running_attempts = m_completion_map.size();
        const size_t max_parallel_downloads = m_options.download_threads;
        auto start_filter = mamba::util::filter(
            m_trackers,
            [&](DownloadTracker& tracker)
            { return running_attempts < max_parallel_downloads && tracker.can_start_transfer(); }
        );

        // Here we loop over all requests contained in filtered m_trackers
        for (auto& tracker : start_filter)
        {
            auto [iter, success] = m_completion_map.insert(
                tracker.prepare_new_attempt(m_curl_handle, *p_params, *p_auth_info, m_options.verbose)
            );
            if (success)
            {
                tracker.set_transfer_started();
                ++running_attempts;
            }
        }
    }

    void Downloader::update_downloads()
    {
        std::size_t still_running = m_curl_handle.perform();

        if (still_running == m_waiting_count)
        {
            m_curl_handle.wait(m_curl_handle.get_timeout());
        }

        while (auto resp = m_curl_handle.pop_message())
        {
            const auto& msg = resp.value();
            if (not msg.m_transfer_done)
            {
                // We are only interested in messages about finished transfers
                continue;
            }

            auto completion_callback_iter = m_completion_map.find(msg.m_handle_id);
            if (completion_callback_iter == m_completion_map.end())
            {
                LOG_ERROR << fmt::format(
                    "Received DONE message from unknown target - running transfers left = {}",
                    still_running
                );
            }
            else
            {
                bool still_waiting = completion_callback_iter->second(
                    m_curl_handle,
                    msg.m_transfer_result
                );
                m_completion_map.erase(completion_callback_iter);
                if (!still_waiting)
                {
                    --m_waiting_count;
                }
            }
        }
    }

    bool Downloader::download_done() const
    {
        return m_waiting_count == 0;
    }

    MultiResult Downloader::build_result() const
    {
        MultiResult result;
        result.reserve(m_trackers.size());
        std::transform(
            m_trackers.begin(),
            m_trackers.end(),
            std::inserter(result, result.begin()),
            [](const DownloadTracker& tracker) { return tracker.get_result(); }
        );
        return result;
    }

    void Downloader::invoke_unexpected_termination() const
    {
        if (m_options.on_unexpected_termination.has_value())
        {
            // We dont want to propagate errors coming from user's callbacks
            [[maybe_unused]] auto result = safe_invoke(m_options.on_unexpected_termination.value());
        }
    }

    /*****************************
     * Public API implementation *
     *****************************/

    void Monitor::observe(MultiRequest& requests, Options& options)
    {
        observe_impl(requests, options);
    }

    void Monitor::on_done()
    {
        on_done_impl();
    }

    void Monitor::on_unexpected_termination()
    {
        on_done_impl();
    }

    MultiResult download(
        MultiRequest requests,
        const mirror_map& mirrors,
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info,
        Options options,
        Monitor* monitor
    )
    {
        if (!params.curl_initialized)
        {
            // TODO: Move this into an object that would be automatically initialized
            // upon construction, and passed by const reference to this function instead
            // of context.
            auto& params_ = const_cast<RemoteFetchParams&>(params);
            init_remote_fetch_params(params_);
        }

        if (monitor)
        {
            monitor->observe(requests, options);
            on_scope_exit guard([monitor]() { monitor->on_done(); });
            Downloader dl(std::move(requests), mirrors, std::move(options), params, auth_info);
            return dl.download();
        }
        else
        {
            Downloader dl(std::move(requests), mirrors, std::move(options), params, auth_info);
            return dl.download();
        }
    }

    Result download(
        Request request,
        const mirror_map& mirrors,
        const RemoteFetchParams& params,
        const specs::AuthenticationDataBase& auth_info,
        Options options,
        Monitor* monitor
    )
    {
        MultiRequest req(1u, std::move(request));
        auto res = download(std::move(req), mirrors, params, auth_info, std::move(options), monitor);
        return std::move(res.front());
    }

    bool check_resource_exists(const std::string& url, const RemoteFetchParams& params)
    {
        if (!params.curl_initialized)
        {
            // TODO: Move this into an object that would be automatically initialized
            // upon construction, and passed by const reference to this function instead
            // of context.
            auto& params_ = const_cast<RemoteFetchParams&>(params);
            init_remote_fetch_params(params_);
        }

        const auto [set_low_speed_opt, set_ssl_no_revoke] = get_env_remote_params(params);

        return curl::check_resource_exists(
            util::file_uri_unc2_to_unc4(url),
            set_low_speed_opt,
            params.connect_timeout_secs,
            set_ssl_no_revoke,
            proxy_match(url, params.proxy_servers),
            params.ssl_verify
        );
    }
}
