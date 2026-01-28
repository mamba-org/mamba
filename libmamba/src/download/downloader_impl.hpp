// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DL_DOWNLOADER_IMPL_HPP
#define MAMBA_DL_DOWNLOADER_IMPL_HPP

#include <chrono>
#include <fstream>
#include <optional>
#include <unordered_map>

#include "mamba/download/downloader.hpp"
#include "mamba/download/mirror_map.hpp"
#include "mamba/download/parameters.hpp"
#include "mamba/specs/authentication_info.hpp"
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
        using on_stop_callback = std::function<bool()>;

        DownloadAttempt() = default;

        DownloadAttempt(
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

        auto create_completion_function() -> completion_function;

        auto request_stop() -> void;

    private:

        // This internal structure stored in an std::unique_ptr is required to guarantee
        // move semantics: some of these functions return lambda capturing the current
        // instance, therefore this latter must be stable in memory.
        struct Impl;
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
        using on_stop_callback = DownloadAttempt::on_stop_callback;

        MirrorAttempt() = default;
        MirrorAttempt(Mirror& mirror, const std::string& url_path, const std::string& spec_sha256);

        expected_t<void> invoke_on_success(const Success& res) const;
        void invoke_on_failure(const Error& res) const;
        void invoke_on_stopped() const;

        void prepare_request(const Request& initial_request);
        auto prepare_attempt(
            CURLHandle& handle,
            CURLMultiHandle& downloader,
            const RemoteFetchParams& params,
            const specs::AuthenticationDataBase& auth_info,
            bool verbose,
            on_success_callback on_success,
            on_failure_callback on_error,
            on_stop_callback on_stop
        ) -> completion_function;

        bool can_start_transfer() const;
        bool has_failed() const;
        bool has_finished() const;
        bool has_stopped() const;

        void set_transfer_started();
        void set_state(bool success);
        void set_state(const Error& res);
        void set_stopped();
        void update_last_content(const Content* content);

        void request_stop();

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
            SEQUENCE_STOPPED,
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

        auto prepare_new_attempt(
            CURLMultiHandle& handle,
            const RemoteFetchParams& params,
            const specs::AuthenticationDataBase& auth_info,
            bool verbose
        ) -> completion_map_entry;

        void request_stop();

        bool has_failed() const;
        bool can_start_transfer() const;
        void set_transfer_started();

        // requires: is_done() == true
        const Result& get_result() const;

        bool is_waiting() const;
        bool is_done() const;
        bool is_ongoing() const;
        void complete_as_stopped();

    private:

        enum class State
        {
            WAITING,
            PREPARING,
            RUNNING,
            STOPPED,
            FINISHED,
            FAILED
        };

        expected_t<void> invoke_on_success(const Success&) const;
        void invoke_on_failure(const Error&) const;
        void invoke_on_stopped() const;

        bool can_try_other_mirror() const;

        void set_state(bool success);
        void set_state(const Error& res);
        void set_error_state();
        void set_stopped();

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
            const RemoteFetchParams& params,
            const specs::AuthenticationDataBase& auth_info
        );

        MultiResult download();

    private:

        void prepare_next_downloads();
        void update_downloads();
        bool download_done() const;
        MultiResult build_result() const;
        void invoke_unexpected_termination() const;

        void request_stop();
        void download_while_stopping();
        void force_stop_waiting_downloads();

        MultiRequest m_requests;
        std::vector<DownloadTracker> m_trackers;
        CURLMultiHandle m_curl_handle;
        Options m_options;
        const mirror_map* p_mirrors;
        const RemoteFetchParams* p_params;
        const specs::AuthenticationDataBase* p_auth_info;
        std::size_t m_waiting_count;

        using completion_function = DownloadTracker::completion_function;
        std::unordered_map<CURLId, completion_function> m_completion_map;
    };
}

#endif
