// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/invoke.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util.hpp"
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

        constexpr std::array<const char*, 6> cert_locations{
            "/etc/ssl/certs/ca-certificates.crt",                 // Debian/Ubuntu/Gentoo etc.
            "/etc/pki/tls/certs/ca-bundle.crt",                   // Fedora/RHEL 6
            "/etc/ssl/ca-bundle.pem",                             // OpenSUSE
            "/etc/pki/tls/cacert.pem",                            // OpenELEC
            "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",  // CentOS/RHEL 7
            "/etc/ssl/cert.pem",                                  // Alpine Linux
        };

        void init_remote_fetch_params(Context::RemoteFetchParams& remote_fetch_params)
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
                else if (remote_fetch_params.ssl_verify == "<system>" && util::on_linux)
                {
                    bool found = false;

                    for (const auto& loc : cert_locations)
                    {
                        if (fs::exists(loc))
                        {
                            remote_fetch_params.ssl_verify = loc;
                            found = true;
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

        EnvRemoteParams get_env_remote_params(const Context& context)
        {
            // TODO: we should probably store set_low_speed_limit and set_ssl_no_revoke in
            // RemoteFetchParams if the request is slower than 30b/s for 60 seconds, cancel.
            const std::string no_low_speed_limit = util::get_env("MAMBA_NO_LOW_SPEED_LIMIT")
                                                       .value_or("0");
            const bool set_low_speed_opt = (no_low_speed_limit == "0");

            const std::string ssl_no_revoke_env = util::get_env("MAMBA_SSL_NO_REVOKE").value_or("0");
            const bool set_ssl_no_revoke = context.remote_fetch_params.ssl_no_revoke
                                           || (ssl_no_revoke_env != "0");

            return { set_low_speed_opt, set_ssl_no_revoke };
        }
    }

    /**********************************
     * DownloadAttempt implementation *
     **********************************/

    DownloadAttempt::DownloadAttempt(
        CURLHandle& handle,
        const MirrorRequest& request,
        CURLMultiHandle& downloader,
        const Context& context,
        on_success_callback success,
        on_failure_callback error
    )
        : p_impl(std::make_unique<
                 Impl>(handle, request, downloader, context, std::move(success), std::move(error)))
    {
    }

    auto DownloadAttempt::create_completion_function() -> completion_function
    {
        return [impl = p_impl.get()](CURLMultiHandle& handle, CURLcode code)
        { return impl->finish_download(handle, code); };
    }

    DownloadAttempt::Impl::Impl(
        CURLHandle& handle,
        const MirrorRequest& request,
        CURLMultiHandle& downloader,
        const Context& context,
        on_success_callback success,
        on_failure_callback error
    )
        : p_handle(&handle)
        , p_request(&request)
        , m_success_callback(std::move(success))
        , m_failure_callback(std::move(error))
        , m_retry_wait_seconds(static_cast<std::size_t>(context.remote_fetch_params.retry_timeout))
    {
        p_stream = make_compression_stream(
            p_request->url,
            [this](char* in, std::size_t size) { return this->write_data(in, size); }
        );
        configure_handle(context);
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
        curl_debug_callback(CURL* /* handle */, curl_infotype type, char* data, size_t size, void* userptr)
        {
            auto* logger = reinterpret_cast<spdlog::logger*>(userptr);
            auto log = Console::hide_secrets(std::string_view(data, size));
            switch (type)
            {
                case CURLINFO_TEXT:
                    logger->info(fmt::format("* {}", log));
                    break;
                case CURLINFO_HEADER_OUT:
                    logger->info(fmt::format("> {}", log));
                    break;
                case CURLINFO_HEADER_IN:
                    logger->info(fmt::format("< {}", log));
                    break;
                default:
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

    void DownloadAttempt::Impl::configure_handle(const Context& context)
    {
        const auto [set_low_speed_opt, set_ssl_no_revoke] = get_env_remote_params(context);

        p_handle->configure_handle(
            util::file_uri_unc2_to_unc4(p_request->url),
            set_low_speed_opt,
            context.remote_fetch_params.connect_timeout_secs,
            set_ssl_no_revoke,
            proxy_match(p_request->url, context.remote_fetch_params.proxy_servers),
            context.remote_fetch_params.ssl_verify
        );

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

        p_handle->set_opt(CURLOPT_VERBOSE, context.output_params.verbosity >= 2);

        configure_handle_headers(context);

        auto logger = spdlog::get("libcurl");
        p_handle->set_opt(CURLOPT_DEBUGFUNCTION, curl_debug_callback);
        p_handle->set_opt(CURLOPT_DEBUGDATA, logger.get());
    }

    void DownloadAttempt::Impl::configure_handle_headers(const Context& context)
    {
        p_handle->reset_headers();

        std::string user_agent = fmt::format(
            "User-Agent: {} {}",
            context.remote_fetch_params.user_agent,
            curl_version()
        );
        p_handle->add_header(user_agent);

        // get url host
        const auto url_handler = util::URL::parse(p_request->url);
        auto host = url_handler.host();
        const auto port = url_handler.port();
        if (port.size())
        {
            host += ":" + port;
        }

        const auto& auth_info = context.authentication_info();
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
     *     - set_state(true) => LAST_REQUEST_FINISHED
     *     - set_state(false) => LAST_REQUEST_FAILED
     *     - set_state(Error with wait_next_retry) => LAST_REQUEST_FAILED
     *     - set_state(Error no wait_next_retry  ) => SEQUENCE_FAILED
     * LAST_REQUEST_FINISHED:
     *     - m_step == m_request_generators.size() ? => SEQUENCE_FINISHED
     * LAST_REQUEST_FAILED:
     *     - m_retries == p_mirror->max_retries ? => SEQUENCE_FAILED
     */
    MirrorAttempt::MirrorAttempt(Mirror& mirror)
        : p_mirror(&mirror)
        , m_request_generators(p_mirror->get_request_generators())
    {
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
        }
    }

    auto MirrorAttempt::prepare_attempt(
        CURLHandle& handle,
        CURLMultiHandle& downloader,
        const Context& context,
        on_success_callback success,
        on_failure_callback error
    ) -> completion_function
    {
        m_state = State::PREPARING_DOWNLOAD;
        m_attempt = DownloadAttempt(
            handle,
            m_request.value(),
            downloader,
            context,
            std::move(success),
            std::move(error)
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
        return m_state == State::SEQUENCE_FINISHED;
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
                            + p_initial_request->mirror_name;
            m_attempt_results.push_back(tl::unexpected(std::move(error)));
        }
    }

    auto DownloadTracker::prepare_new_attempt(CURLMultiHandle& handle, const Context& context)
        -> completion_map_entry
    {
        m_state = State::PREPARING;

        m_mirror_attempt.prepare_request(*p_initial_request);
        auto completion_func = m_mirror_attempt.prepare_attempt(
            m_handle,
            handle,
            context,
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
        return m_attempt_results.back();
    }

    expected_t<void> DownloadTracker::invoke_on_success(const Success& res) const
    {
        if (p_initial_request->on_success.has_value())
        {
            auto ret = safe_invoke(p_initial_request->on_success.value(), res);
            return ret.has_value() ? ret.value() : forward_error(ret);
        }
        return expected_t<void>();
    }

    void DownloadTracker::invoke_on_failure(const Error& res) const
    {
        if (p_initial_request->on_failure.has_value())
        {
            safe_invoke(p_initial_request->on_failure.value(), res);
        }
    }

    bool DownloadTracker::is_waiting() const
    {
        return m_state == State::WAITING;
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
            m_mirror_attempt = MirrorAttempt(*mirror);
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
                [this, iteration](const auto& mirror) {
                    return iteration > mirror->failed_transfers()
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
        return mirror->successful_transfers() == 0
               && mirror->failed_transfers() >= mirror->max_retries();
    }

    /*****************************
     * DOWNLOADER IMPLEMENTATION *
     *****************************/

    Downloader::Downloader(
        MultiRequest requests,
        const mirror_map& mirrors,
        Options options,
        const Context& context
    )
        : m_requests(std::move(requests))
        , p_mirrors(&mirrors)
        , m_options(std::move(options))
        , p_context(&context)
        , m_curl_handle(context.threads_params.download_threads)
        , m_trackers()
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
        std::size_t max_retries = static_cast<std::size_t>(context.remote_fetch_params.max_retries);
        DownloadTrackerOptions tracker_options{ max_retries, options.fail_fast };
        std::transform(
            m_requests.begin(),
            m_requests.end(),
            std::back_inserter(m_trackers),
            [tracker_options, this](const Request& req) {
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

    MultiResult Downloader::download()
    {
        while (!download_done())
        {
            if (is_sig_interrupted())
            {
                invoke_unexpected_termination();
                break;
            }
            prepare_next_downloads();
            update_downloads();
        }
        return build_result();
    }

    void Downloader::prepare_next_downloads()
    {
        size_t running_attempts = m_completion_map.size();
        const size_t max_parallel_downloads = p_context->threads_params.download_threads;
        auto start_filter = mamba::util::filter(
            m_trackers,
            [&](DownloadTracker& tracker)
            { return running_attempts < max_parallel_downloads && tracker.can_start_transfer(); }
        );

        for (auto& tracker : start_filter)
        {
            auto [iter, success] = m_completion_map.insert(
                tracker.prepare_new_attempt(m_curl_handle, *p_context)
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
            if (!msg.m_transfer_done)
            {
                // We are only interested in messages about finished transfers
                continue;
            }

            auto completion_callback = m_completion_map.find(msg.m_handle_id);
            if (completion_callback == m_completion_map.end())
            {
                spdlog::error(
                    "Received DONE message from unknown target - running transfers left = {}",
                    still_running
                );
            }
            else
            {
                bool still_waiting = completion_callback->second(m_curl_handle, msg.m_transfer_result);
                m_completion_map.erase(completion_callback);
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
            safe_invoke(m_options.on_unexpected_termination.value());
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
        const Context& context,
        Options options,
        Monitor* monitor
    )
    {
        if (!context.remote_fetch_params.curl_initialized)
        {
            // TODO: Move this into an object that would be automatically initialized
            // upon construction, and passed by const reference to this function instead
            // of context.
            Context& ctx = const_cast<Context&>(context);
            init_remote_fetch_params(ctx.remote_fetch_params);
        }

        if (monitor)
        {
            monitor->observe(requests, options);
            on_scope_exit guard([monitor]() { monitor->on_done(); });
            Downloader dl(std::move(requests), mirrors, std::move(options), context);
            return dl.download();
        }
        else
        {
            Downloader dl(std::move(requests), mirrors, std::move(options), context);
            return dl.download();
        }
    }

    Result download(
        Request request,
        const mirror_map& mirrors,
        const Context& context,
        Options options,
        Monitor* monitor
    )
    {
        MultiRequest req(1u, std::move(request));
        auto res = download(std::move(req), mirrors, context, std::move(options), monitor);
        return std::move(res.front());
    }

    bool check_resource_exists(const std::string& url, const Context& context)
    {
        if (!context.remote_fetch_params.curl_initialized)
        {
            // TODO: Move this into an object that would be automatically initialized
            // upon construction, and passed by const reference to this function instead
            // of context.
            Context& ctx = const_cast<Context&>(context);
            init_remote_fetch_params(ctx.remote_fetch_params);
        }

        const auto [set_low_speed_opt, set_ssl_no_revoke] = get_env_remote_params(context);

        return curl::check_resource_exists(
            util::file_uri_unc2_to_unc4(url),
            set_low_speed_opt,
            context.remote_fetch_params.connect_timeout_secs,
            set_ssl_no_revoke,
            proxy_match(url, context.remote_fetch_params.proxy_servers),
            context.remote_fetch_params.ssl_verify
        );
    }
}
