// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_DOWNLOAD_IMPL_HPP
#define MAMBA_CORE_DOWNLOAD_IMPL_HPP

#include <chrono>
#include <unordered_map>

#include "mamba/core/download.hpp"

#include "compression.hpp"
#include "curl.hpp"

namespace mamba
{
    enum class DownloadState
    {
        WAITING,
        PREPARING,
        RUNNING,
        FINISHED,
        FAILED
    };

    class DownloadAttempt
    {
    public:

        using completion_function = std::function<bool(CURLMultiHandle&, CURLcode)>;
        using on_success_callback = std::function<bool(DownloadSuccess)>;
        using on_failure_callback = std::function<bool(DownloadError)>;

        explicit DownloadAttempt(const DownloadRequest& request);

        CURLId prepare_download(
            CURLMultiHandle& downloader,
            const Context& context,
            on_success_callback success,
            on_failure_callback error
        );

        auto create_completion_function() -> completion_function;

    private:

        bool finish_download(CURLMultiHandle& downloader, CURLcode code);
        void clean_attempt(CURLMultiHandle& downloader, bool erase_downloaded);
        void invoke_progress_callback(const DownloadEvent&) const;

        void configure_handle(const Context& context);
        void configure_handle_headers(const Context& context);

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
        DownloadError build_download_error(CURLcode code) const;
        DownloadError build_download_error(TransferData data) const;
        DownloadSuccess build_download_success(TransferData data) const;

        const DownloadRequest* p_request;
        CURLHandle m_handle;
        on_success_callback m_success_callback;
        on_failure_callback m_failure_callback;
        std::size_t m_retry_wait_seconds;
        std::unique_ptr<CompressionStream> p_stream;
        std::ofstream m_file;
        std::string m_cache_control;
        std::string m_etag;
        std::string m_last_modified;
    };

    struct DownloadTrackerOptions
    {
        std::size_t max_retries = 0;
        bool fail_fast = false;
    };

    class DownloadTracker
    {
    public:

        using completion_function = DownloadAttempt::completion_function;
        using completion_map_entry = std::pair<CURLId, completion_function>;

        DownloadTracker(const DownloadRequest& request, DownloadTrackerOptions options);

        auto prepare_new_attempt(CURLMultiHandle& handle, const Context& context)
            -> completion_map_entry;

        bool can_start_transfer() const;
        void set_transfer_started();

        const DownloadResult& get_result() const;

    private:

        expected_t<void> invoke_on_success(const DownloadSuccess&) const;
        void invoke_on_failure(const DownloadError&) const;

        bool is_waiting() const;

        void set_state(bool success);
        void set_state(const DownloadError& res);

        void throw_if_required(const expected_t<void>&);
        void throw_if_required(const DownloadError&);

        void save(DownloadSuccess&&);
        void save(DownloadError&&);

        const DownloadRequest* p_request;
        DownloadTrackerOptions m_options;
        DownloadAttempt m_attempt;
        std::vector<DownloadResult> m_attempt_results;
        DownloadState m_state;
        using time_point_t = std::chrono::steady_clock::time_point;
        std::optional<time_point_t> m_next_retry;
    };

    class Downloader
    {
    public:

        explicit Downloader(MultiDownloadRequest requests, DownloadOptions options, const Context& context);

        MultiDownloadResult download();

    private:

        void prepare_next_downloads();
        void update_downloads();
        bool download_done() const;
        MultiDownloadResult build_result() const;
        void invoke_unexpected_termination() const;

        MultiDownloadRequest m_requests;
        DownloadOptions m_options;
        const Context* p_context;
        CURLMultiHandle m_curl_handle;
        std::vector<DownloadTracker> m_trackers;
        size_t m_waiting_count;

        using completion_function = DownloadTracker::completion_function;
        std::unordered_map<CURLId, completion_function> m_completion_map;
    };
}

#endif
