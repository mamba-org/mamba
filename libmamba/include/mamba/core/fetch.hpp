// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_FETCH_HPP
#define MAMBA_CORE_FETCH_HPP

#include <string>
#include <vector>

///////////////////////////////////////
// TODO to remove, kept here until not needed anymore/complete libcurl isolation is done
extern "C"
{
#include <curl/curl.h>
}

////////////////////////////////////

#include "progress_bar.hpp"
#include "validate.hpp"

namespace mamba
{
    struct ZstdStream;
    struct Bzip2Stream;

    class CURLHandle;

    class DownloadTarget
    {
    public:

        DownloadTarget(const std::string& name, const std::string& url, const std::string& filename);
        ~DownloadTarget();

        DownloadTarget(const DownloadTarget&) = delete;
        DownloadTarget& operator=(const DownloadTarget&) = delete;
        DownloadTarget(DownloadTarget&&) = delete;
        DownloadTarget& operator=(DownloadTarget&&) = delete;

        static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* self);
        static size_t header_callback(char* buffer, size_t size, size_t nitems, void* self);

        static int progress_callback(
            void*,
            curl_off_t total_to_download,
            curl_off_t now_downloaded,
            curl_off_t,
            curl_off_t
        );
        void set_mod_etag_headers(const std::string& mod, const std::string& etag);
        void set_progress_bar(ProgressProxy progress_proxy);
        void set_expected_size(std::size_t size);
        void set_head_only(bool yes);

        const std::string& get_name() const;
        const std::string& get_url() const;

        const std::string& get_etag() const;
        const std::string& get_mod() const;
        const std::string& get_cache_control() const;

        std::size_t get_expected_size() const;
        int get_http_status() const;
        std::size_t get_downloaded_size() const;

        std::size_t get_speed();

        void init_curl_ssl();
        void init_curl_target(const std::string& url);

        template <class C>
        inline void set_finalize_callback(bool (C::*cb)(const DownloadTarget&), C* data)
        {
            m_finalize_callback = std::bind(cb, data, std::placeholders::_1);
        }

        void set_ignore_failure(bool yes)
        {
            m_ignore_failure = yes;
        }

        bool get_ignore_failure() const
        {
            return m_ignore_failure;
        }

        void set_result(CURLcode r);
        std::size_t get_result() const;

        bool resource_exists();
        bool perform();
        CURL* handle();

        bool finalize();
        std::string get_transfer_msg();

        bool can_retry();
        CURL* retry();

        std::chrono::steady_clock::time_point progress_throttle_time() const;
        void set_progress_throttle_time(const std::chrono::steady_clock::time_point& time);

    private:

        std::unique_ptr<ZstdStream> m_zstd_stream;
        std::unique_ptr<Bzip2Stream> m_bzip2_stream;
        std::unique_ptr<CURLHandle> m_curl_handle;
        std::function<bool(const DownloadTarget&)> m_finalize_callback;

        std::string m_name, m_filename, m_url;

        int m_http_status;
        std::size_t m_downloaded_size;
        char* m_effective_url;

        CURLcode m_result;  // Enum range from 0 to 99

        std::string m_etag, m_mod, m_cache_control;

        // validation
        std::size_t m_expected_size;

        // retry & backoff
        std::chrono::steady_clock::time_point m_next_retry;
        std::size_t m_retry_wait_seconds;
        std::size_t m_retries;

        bool m_has_progress_bar;
        bool m_ignore_failure;

        ProgressProxy m_progress_bar;

        std::ofstream m_file;

        static std::size_t get_default_retry_timeout();
        static void init_curl_handle(CURL* handle, const std::string& url);
        std::function<void(ProgressBarRepr&)> download_repr();

        std::chrono::steady_clock::time_point m_progress_throttle_time;
    };

    class MultiDownloadTarget
    {
    public:

        MultiDownloadTarget();
        ~MultiDownloadTarget();

        void add(DownloadTarget* target);
        bool check_msgs(bool failfast);
        bool download(int options);

    private:

        std::vector<DownloadTarget*> m_targets;
        std::vector<DownloadTarget*> m_retry_targets;
        CURLM* m_handle;
    };

    const int MAMBA_DOWNLOAD_FAILFAST = 1 << 0;
    const int MAMBA_DOWNLOAD_SORT = 1 << 1;
    const int MAMBA_NO_CLEAR_PROGRESS_BARS = 1 << 2;
}  // namespace mamba

#endif  // MAMBA_FETCH_HPP
