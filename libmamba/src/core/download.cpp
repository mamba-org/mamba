#include "mamba/core/download.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/iterator.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url.hpp"

#include "curl.hpp"
#include "download_impl.hpp"

namespace mamba
{

    /**********************************
     * DownloadAttempt implementation *
     **********************************/

    DownloadAttempt::DownloadAttempt(const DownloadRequest& request)
        : p_request(&request)
    {
        p_stream = make_compression_stream(
            p_request->url,
            [this](char* in, std::size_t size) { return this->write_data(in, size); }
        );
        m_retry_wait_seconds = std::size_t(0);
    }

    CURLId DownloadAttempt::prepare_download(
        CURLMultiHandle& downloader,
        const Context& context,
        on_success_callback success,
        on_failure_callback error
    )
    {
        m_retry_wait_seconds = static_cast<std::size_t>(context.remote_fetch_params.retry_timeout);
        configure_handle(context);
        downloader.add_handle(m_handle);
        m_success_callback = std::move(success);
        m_failure_callback = std::move(error);
        return m_handle.get_id();
    }


    namespace
    {
        bool is_http_status_ok(int http_status)
        {
            // Note: http_status == 0 for files
            return http_status / 100 == 2 || http_status == 304 || http_status == 0;
        }
    }

    bool DownloadAttempt::finish_download(CURLMultiHandle& downloader, CURLcode code)
    {
        if (!CURLHandle::is_curl_res_ok(code))
        {
            DownloadError error = build_download_error(code);
            clean_attempt(downloader, true);
            invoke_progress_callback(error);
            return m_failure_callback(std::move(error));
        }
        else
        {
            TransferData data = get_transfer_data();
            if (!is_http_status_ok(data.http_status))
            {
                DownloadError error = build_download_error(std::move(data));
                clean_attempt(downloader, true);
                invoke_progress_callback(error);
                return m_failure_callback(std::move(error));
            }
            else
            {
                DownloadSuccess success = build_download_success(std::move(data));
                clean_attempt(downloader, false);
                invoke_progress_callback(success);
                return m_success_callback(std::move(success));
            }
        }
    }


    void DownloadAttempt::clean_attempt(CURLMultiHandle& downloader, bool erase_downloaded)
    {
        downloader.remove_handle(m_handle);
        m_handle.reset_handle();

        if (m_file.is_open())
        {
            m_file.close();
        }
        if (erase_downloaded && fs::exists(p_request->filename))
        {
            fs::remove(p_request->filename);
        }

        m_cache_control.clear();
        m_etag.clear();
        m_last_modified.clear();
    }

    void DownloadAttempt::invoke_progress_callback(const DownloadEvent& event) const
    {
        if (p_request->progress.has_value())
        {
            p_request->progress.value()(event);
        }
    }

    auto DownloadAttempt::create_completion_function() -> completion_function
    {
        return [this](CURLMultiHandle& handle, CURLcode code)
        { return this->finish_download(handle, code); };
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

    void DownloadAttempt::configure_handle(const Context& context)
    {
        // TODO: we should probably store set_low_speed_limit and set_ssl_no_revoke in
        // RemoteFetchParams if the request is slower than 30b/s for 60 seconds, cancel.
        const std::string no_low_speed_limit = std::getenv("MAMBA_NO_LOW_SPEED_LIMIT")
                                                   ? std::getenv("MAMBA_NO_LOW_SPEED_LIMIT")
                                                   : "0";
        const bool set_low_speed_opt = (no_low_speed_limit == "0");

        const std::string ssl_no_revoke_env = std::getenv("MAMBA_SSL_NO_REVOKE")
                                                  ? std::getenv("MAMBA_SSL_NO_REVOKE")
                                                  : "0";
        const bool set_ssl_no_revoke = context.remote_fetch_params.ssl_no_revoke
                                       || (ssl_no_revoke_env != "0");

        m_handle.configure_handle(
            p_request->url,
            set_low_speed_opt,
            context.remote_fetch_params.connect_timeout_secs,
            set_ssl_no_revoke,
            proxy_match(p_request->url, context.remote_fetch_params.proxy_servers),
            context.remote_fetch_params.ssl_verify
        );

        m_handle.set_opt(CURLOPT_NOBODY, p_request->head_only);

        m_handle.set_opt(CURLOPT_HEADERFUNCTION, &DownloadAttempt::curl_header_callback);
        m_handle.set_opt(CURLOPT_HEADERDATA, this);

        m_handle.set_opt(CURLOPT_WRITEFUNCTION, &DownloadAttempt::curl_write_callback);
        m_handle.set_opt(CURLOPT_WRITEDATA, this);

        if (p_request->progress.has_value())
        {
            m_handle.set_opt(CURLOPT_XFERINFOFUNCTION, &DownloadAttempt::curl_progress_callback);
            m_handle.set_opt(CURLOPT_XFERINFODATA, this);
            m_handle.set_opt(CURLOPT_NOPROGRESS, 0L);
        }

        if (util::ends_with(p_request->url, ".json"))
        {
            // accept all encodings supported by the libcurl build
            m_handle.set_opt(CURLOPT_ACCEPT_ENCODING, "");
            m_handle.add_header("Content-Type: application/json");
        }

        m_handle.set_opt(CURLOPT_VERBOSE, context.output_params.verbosity >= 2);

        configure_handle_headers(context);

        auto logger = spdlog::get("libcurl");
        m_handle.set_opt(CURLOPT_DEBUGFUNCTION, curl_debug_callback);
        m_handle.set_opt(CURLOPT_DEBUGDATA, logger.get());
    }

    void DownloadAttempt::configure_handle_headers(const Context& context)
    {
        m_handle.reset_headers();

        std::string user_agent = fmt::format(
            "User-Agent: {} {}",
            context.remote_fetch_params.user_agent,
            curl_version()
        );
        m_handle.add_header(user_agent);

        // get url host
        const auto url_handler = util::URL::parse(p_request->url);
        auto host = url_handler.host();
        const auto port = url_handler.port();
        if (port.size())
        {
            host += ":" + port;
        }

        if (context.authentication_info().count(host))
        {
            const auto& auth = context.authentication_info().at(host);
            if (std::holds_alternative<specs::BearerToken>(auth))
            {
                m_handle.add_header(
                    fmt::format("Authorization: Bearer {}", std::get<specs::BearerToken>(auth).token)
                );
            }
        }

        if (p_request->etag.has_value())
        {
            m_handle.add_header("If-None-Match:" + p_request->etag.value());
        }

        if (p_request->last_modified.has_value())
        {
            m_handle.add_header("If-Modified-Since:" + p_request->last_modified.value());
        }

        m_handle.set_opt_header();
    }

    size_t DownloadAttempt::write_data(char* buffer, size_t size)
    {
        if (!m_file.is_open())
        {
            m_file = open_ofstream(p_request->filename, std::ios::binary);
            if (!m_file)
            {
                LOG_ERROR << "Could not open file for download " << p_request->filename << ": "
                          << strerror(errno);
                // Return a size _different_ than the expected write size to signal an error
                return size + 1;
            }
        }

        m_file.write(buffer, static_cast<std::streamsize>(size));

        if (!m_file)
        {
            LOG_ERROR << "Could not write to file " << p_request->filename << ": " << strerror(errno);
            // Return a size _different_ than the expected write size to signal an error
            return size + 1;
        }
        return size;
    }

    size_t
    DownloadAttempt::curl_header_callback(char* buffer, size_t size, size_t nbitems, void* self)
    {
        auto* s = reinterpret_cast<DownloadAttempt*>(self);

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

    size_t DownloadAttempt::curl_write_callback(char* buffer, size_t size, size_t nbitems, void* self)
    {
        return reinterpret_cast<DownloadAttempt*>(self)->p_stream->write(buffer, size * nbitems);
    }

    int DownloadAttempt::curl_progress_callback(
        void* f,
        curl_off_t total_to_download,
        curl_off_t now_downloaded,
        curl_off_t,
        curl_off_t
    )
    {
        auto* self = reinterpret_cast<DownloadAttempt*>(f);
        self->p_request->progress.value()(DownloadProgress{
            static_cast<std::size_t>(now_downloaded),
            static_cast<std::size_t>(total_to_download) });
        return 0;
    }

    namespace http
    {
        static constexpr int PAYLOAD_TOO_LARGE = 413;
        static constexpr int TOO_MANY_REQUESTS = 429;
        static constexpr int INTERNAL_SERVER_ERROR = 500;
        static constexpr int ARBITRARY_ERROR = 10000;
    }

    bool DownloadAttempt::can_retry(CURLcode code) const
    {
        return m_handle.can_retry(code) && !util::starts_with(p_request->url, "file://");
    }

    bool DownloadAttempt::can_retry(const TransferData& data) const
    {
        return (data.http_status == http::PAYLOAD_TOO_LARGE
                || data.http_status == http::TOO_MANY_REQUESTS
                || data.http_status >= http::INTERNAL_SERVER_ERROR)
               && !util::starts_with(p_request->url, "file://");
    }

    TransferData DownloadAttempt::get_transfer_data() const
    {
        return {
            /* .http_status = */ m_handle.get_info<int>(CURLINFO_RESPONSE_CODE)
                .value_or(http::ARBITRARY_ERROR),
            /* .effective_url = */ m_handle.get_info<char*>(CURLINFO_EFFECTIVE_URL).value(),
            /* .dwonloaded_size = */ m_handle.get_info<std::size_t>(CURLINFO_SIZE_DOWNLOAD_T).value_or(0),
            /* .average_speed = */ m_handle.get_info<std::size_t>(CURLINFO_SPEED_DOWNLOAD_T).value_or(0)
        };
    }

    DownloadError DownloadAttempt::build_download_error(CURLcode code) const
    {
        DownloadError error;
        std::stringstream strerr;
        strerr << "Download error (" << code << ") " << m_handle.get_res_error(code) << " ["
               << m_handle.get_curl_effective_url() << "]\n"
               << m_handle.get_error_buffer();
        error.message = strerr.str();

        if (can_retry(code))
        {
            error.retry_wait_seconds = m_retry_wait_seconds;
        }
        return error;
    }

    DownloadError DownloadAttempt::build_download_error(TransferData data) const
    {
        DownloadError error;
        if (can_retry(data))
        {
            error.retry_wait_seconds = m_handle.get_info<std::size_t>(CURLINFO_RETRY_AFTER)
                                           .value_or(m_retry_wait_seconds);
        }
        error.message = build_transfer_message(data.http_status, data.effective_url, data.downloaded_size);
        error.transfer = std::move(data);
        return error;
    }

    DownloadSuccess DownloadAttempt::build_download_success(TransferData data) const
    {
        return { /*.filename = */ p_request->filename,
                 /*.transfer = */ std::move(data),
                 /*.cache_control = */ m_cache_control,
                 /*.etag = */ m_etag,
                 /*.last_modified = */ m_last_modified };
    }

    /**********************************
     * DownloadTracker implementation *
     **********************************/

    DownloadTracker::DownloadTracker(const DownloadRequest& request, DownloadTrackerOptions options)
        : p_request(&request)
        , m_options(std::move(options))
        , m_attempt(request)
        , m_attempt_results()
        , m_state(DownloadState::WAITING)
        , m_next_retry(std::nullopt)
    {
    }

    auto DownloadTracker::prepare_new_attempt(CURLMultiHandle& handle, const Context& context)
        -> completion_map_entry
    {
        m_next_retry = std::nullopt;

        CURLId id = m_attempt.prepare_download(
            handle,
            context,
            [this](DownloadSuccess res)
            {
                bool finalize_res = invoke_on_success(res);
                set_state(finalize_res);
                throw_if_required(res);
                save(std::move(res));
                return is_waiting();
            },
            [this](DownloadError res)
            {
                invoke_on_failure(res);
                set_state(res);
                throw_if_required(res);
                save(std::move(res));
                return is_waiting();
            }
        );
        return { id, m_attempt.create_completion_function() };
    }

    bool DownloadTracker::can_start_transfer() const
    {
        return is_waiting()
               && (!m_next_retry.has_value()
                   || m_next_retry.value() < std::chrono::steady_clock::now());
    }

    const DownloadResult& DownloadTracker::get_result() const
    {
        return m_attempt_results.back();
    }

    bool DownloadTracker::invoke_on_success(const DownloadSuccess& res) const
    {
        if (p_request->on_success.has_value())
        {
            return p_request->on_success.value()(res);
        }
        return true;
    }

    void DownloadTracker::invoke_on_failure(const DownloadError& res) const
    {
        if (p_request->on_failure.has_value())
        {
            p_request->on_failure.value()(res);
        }
    }

    bool DownloadTracker::is_waiting() const
    {
        return m_state == DownloadState::WAITING;
    }

    void DownloadTracker::set_state(bool success)
    {
        if (success)
        {
            m_state = DownloadState::FINISHED;
        }
        else
        {
            if (m_attempt_results.size() < m_options.max_retries)
            {
                m_state = DownloadState::WAITING;
            }
            else
            {
                m_state = DownloadState::FAILED;
            }
        }
    }

    void DownloadTracker::set_state(const DownloadError& res)
    {
        if (res.retry_wait_seconds.has_value())
        {
            if (m_attempt_results.size() < m_options.max_retries)
            {
                m_state = DownloadState::WAITING;
                m_next_retry = std::chrono::steady_clock::now()
                               + std::chrono::seconds(res.retry_wait_seconds.value());
            }
            else
            {
                m_state = DownloadState::FAILED;
            }
        }
        else
        {
            m_state = DownloadState::FAILED;
        }
    }

    void DownloadTracker::throw_if_required(const DownloadSuccess& res)
    {
        if (m_state == DownloadState::FAILED && !p_request->ignore_failure && m_options.fail_fast)
        {
            throw std::runtime_error(
                "Multi-download failed. Reason: "
                + build_transfer_message(
                    res.transfer.http_status,
                    res.transfer.effective_url,
                    res.transfer.downloaded_size
                )
            );
        }
    }

    void DownloadTracker::throw_if_required(const DownloadError& res)
    {
        if (m_state == DownloadState::FAILED && !p_request->ignore_failure)
        {
            throw std::runtime_error(res.message);
        }
    }

    void DownloadTracker::save(DownloadSuccess&& res)
    {
        res.attempt_number = m_attempt_results.size() + std::size_t(1);
        m_attempt_results.push_back(DownloadResult(std::move(res)));
    }

    void DownloadTracker::save(DownloadError&& res)
    {
        res.attempt_number = m_attempt_results.size() + std::size_t(1);
        m_attempt_results.push_back(tl::unexpected(std::move(res)));
    }

    /*****************************
     * DOWNLOADER IMPLEMENTATION *
     *****************************/

    Downloader::Downloader(MultiDownloadRequest requests, DownloadOptions options, const Context& context)
        : m_requests(std::move(requests))
        , m_options(std::move(options))
        , p_context(&context)
        , m_curl_handle(context.threads_params.download_threads)
        , m_trackers()
    {
        if (m_options.sort)
        {
            std::sort(
                m_requests.requests.begin(),
                m_requests.requests.end(),
                [](const DownloadRequest& a, const DownloadRequest& b) -> bool
                { return a.expected_size.value_or(SIZE_MAX) > b.expected_size.value_or(SIZE_MAX); }
            );
        }

        m_trackers.reserve(m_requests.requests.size());
        std::size_t max_retries = static_cast<std::size_t>(context.remote_fetch_params.max_retries);
        DownloadTrackerOptions tracker_options{ max_retries, options.fail_fast };
        std::transform(
            m_requests.requests.begin(),
            m_requests.requests.end(),
            std::inserter(m_trackers, m_trackers.begin()),
            [tracker_options](const DownloadRequest& req)
            { return DownloadTracker(req, tracker_options); }
        );
        m_waiting_count = m_trackers.size();
    }

    MultiDownloadResult Downloader::download()
    {
        while (!download_done())
        {
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
                ++running_attempts;
            }
        }
    }

    void Downloader::update_downloads()
    {
        std::size_t still_running = m_curl_handle.perform();
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

    MultiDownloadResult Downloader::build_result() const
    {
        DownloadResultList result;
        result.reserve(m_trackers.size());
        std::transform(
            m_trackers.begin(),
            m_trackers.end(),
            std::inserter(result, result.begin()),
            [](const DownloadTracker& tracker) { return tracker.get_result(); }
        );
        return { result };
    }

    /*****************************
     * Public API implementation *
     *****************************/

    DownloadRequest::DownloadRequest(
        const std::string& lname,
        const std::string& lurl,
        const std::string& lfilename,
        bool lhead_only,
        bool lignore_failure
    )
        : name(lname)
        , url(lurl)
        , filename(lfilename)
        , head_only(lhead_only)
        , ignore_failure(lignore_failure)
    {
    }

    MultiDownloadResult
    download(MultiDownloadRequest requests, const Context& context, DownloadOptions options)
    {
        Downloader dl(std::move(requests), std::move(options), context);
        return dl.download();
    }
}
