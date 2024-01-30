// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DL_DOWNLOADER_IMPL_HPP
#define MAMBA_DL_DOWNLOADER_IMPL_HPP

#include <chrono>
#include <optional>
#include <unordered_map>

#include "mamba/download/downloader.hpp"
#include "mamba/download/mirror_map.hpp"
#include "mamba/util/flat_set.hpp"

#include "compression.hpp"
#include "curl.hpp"

namespace mamba::download
{
    /*
     * DownloadAttempt
     */
    class DownloadAttempt
    {
    public:

        using completion_function = std::function<bool(CURLMultiHandle&, CURLcode)>;
        using on_success_callback = std::function<bool(Success)>;
        using on_failure_callback = std::function<bool(Error)>;

        DownloadAttempt() = default;

        DownloadAttempt(
            CURLHandle& handle,
            const MirrorRequest& request,
            CURLMultiHandle& downloader,
            const Context& context,
            on_success_callback success,
            on_failure_callback error
        );

        auto create_completion_function() -> completion_function;

    private:

        // This internal structure stored in an std::unique_ptr is required to guarantee
        // move semantics: some of these functions return lambda capturing the current
        // instance, therefore this latter must be stable in memory.
        struct Impl
        {
            Impl(
                CURLHandle& handle,
                const MirrorRequest& request,
                CURLMultiHandle& downloader,
                const Context& context,
                on_success_callback success,
                on_failure_callback error
            );

            bool finish_download(CURLMultiHandle& downloader, CURLcode code);
            void clean_attempt(CURLMultiHandle& downloader, bool erase_downloaded);
            void invoke_progress_callback(const Event&) const;

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
            Error build_download_error(CURLcode code) const;
            Error build_download_error(TransferData data) const;
            Success build_download_success(TransferData data) const;

            CURLHandle* p_handle = nullptr;
            const MirrorRequest* p_request = nullptr;
            on_success_callback m_success_callback;
            on_failure_callback m_failure_callback;
            std::size_t m_retry_wait_seconds = std::size_t(0);
            std::unique_ptr<CompressionStream> p_stream = nullptr;
            std::ofstream m_file;
            mutable std::string m_response = "";
            std::string m_cache_control;
            std::string m_etag;
            std::string m_last_modified;
        };

        std::unique_ptr<Impl> p_impl = nullptr;
    };

    /*
     * MirrorAttempt
     *
     * Manages the sequence of requests required to download an artifact
     * on a mirror.
     *
     */
    class MirrorAttempt
    {
    public:

        using completion_function = DownloadAttempt::completion_function;
        using on_success_callback = DownloadAttempt::on_success_callback;
        using on_failure_callback = DownloadAttempt::on_failure_callback;

        MirrorAttempt() = default;
        explicit MirrorAttempt(Mirror& mirror);

        void prepare_request(const Request& initial_request);
        auto prepare_attempt(
            CURLHandle& handle,
            CURLMultiHandle& downloader,
            const Context& context,
            on_success_callback success,
            on_failure_callback error
        ) -> completion_function;

        bool can_start_transfer() const;
        bool has_failed() const;
        bool has_finished() const;

        void set_transfer_started();
        void set_state(bool success);
        void set_state(const Error& res);
        void update_last_content(const Content* content);

    private:

        enum class State
        {
            WAITING_SEQUENCE_START,
            PREPARING_DOWNLOAD,
            RUNNING_DOWNLOAD,
            LAST_REQUEST_FINISHED,
            LAST_REQUEST_FAILED,
            SEQUENCE_FINISHED,
            SEQUENCE_FAILED,
        };

        bool can_retry() const;
        void update_transfers_done(bool success);

        Mirror* p_mirror = nullptr;
        State m_state = State::WAITING_SEQUENCE_START;

        using request_generator_list = Mirror::request_generator_list;
        request_generator_list m_request_generators;
        size_t m_step = 0;

        std::optional<MirrorRequest> m_request;
        DownloadAttempt m_attempt;
        const Content* p_last_content = nullptr;

        using time_point_t = std::chrono::steady_clock::time_point;
        std::optional<time_point_t> m_next_retry;
        size_t m_retries = 0;
    };

    struct DownloadTrackerOptions
    {
        std::size_t max_mirror_tries = 0;
        bool fail_fast = false;
    };

    class DownloadTracker
    {
    public:

        using completion_function = DownloadAttempt::completion_function;
        using completion_map_entry = std::pair<CURLId, completion_function>;

        DownloadTracker(
            const Request& request,
            const mirror_set_view& mirror_set,
            DownloadTrackerOptions options
        );

        auto prepare_new_attempt(CURLMultiHandle& handle, const Context& context)
            -> completion_map_entry;

        bool has_failed() const;
        bool can_start_transfer() const;
        void set_transfer_started();

        const Result& get_result() const;

    private:

        enum class State
        {
            WAITING,
            PREPARING,
            RUNNING,
            FINISHED,
            FAILED
        };

        expected_t<void> invoke_on_success(const Success&) const;
        void invoke_on_failure(const Error&) const;

        bool is_waiting() const;
        bool can_try_other_mirror() const;

        void set_state(bool success);
        void set_state(const Error& res);
        void set_error_state();

        /**
         * Invoked when the download succeeded but the download callback
         * failed.
         */
        void throw_if_required(const expected_t<void>&);

        /**
         * Invoked when the download failed.
         */
        void throw_if_required(const Error&);

        void save(Success&&);
        void save(Error&&);

        void prepare_mirror_attempt();
        Mirror* select_new_mirror() const;
        bool has_tried_mirror(Mirror* mirror) const;
        bool is_bad_mirror(Mirror* mirror) const;

        CURLHandle m_handle;
        const Request* p_initial_request;
        mirror_set_view m_mirror_set;
        DownloadTrackerOptions m_options;

        State m_state;
        std::vector<Result> m_attempt_results;
        util::flat_set<MirrorID> m_tried_mirrors;
        MirrorAttempt m_mirror_attempt;
    };

    class Downloader
    {
    public:

        explicit Downloader(
            MultiRequest requests,
            const mirror_map& mirrors,
            Options options,
            const Context& context
        );

        MultiResult download();

    private:

        void prepare_next_downloads();
        void update_downloads();
        bool download_done() const;
        MultiResult build_result() const;
        void invoke_unexpected_termination() const;

        MultiRequest m_requests;
        const mirror_map* p_mirrors;
        Options m_options;
        const Context* p_context;
        CURLMultiHandle m_curl_handle;
        std::vector<DownloadTracker> m_trackers;
        size_t m_waiting_count;

        using completion_function = DownloadTracker::completion_function;
        std::unordered_map<CURLId, completion_function> m_completion_map;
    };
}

#endif
