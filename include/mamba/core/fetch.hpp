// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_FETCH_HPP
#define MAMBA_CORE_FETCH_HPP

extern "C"
{
#include <archive.h>
#include <curl/curl.h>
}

#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "output.hpp"
#include "validate.hpp"

namespace mamba
{
    void init_curl_ssl();

    class DownloadTarget
    {
    public:
        DownloadTarget() = default;
        DownloadTarget(const std::string& name,
                       const std::string& url,
                       const std::string& filename);
        ~DownloadTarget();

        static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* self);
        static size_t header_callback(char* buffer, size_t size, size_t nitems, void* self);

        int progress_callback(
            void*, curl_off_t total_to_download, curl_off_t now_downloaded, curl_off_t, curl_off_t);
        void set_mod_etag_headers(const nlohmann::json& mod_etag);
        void set_progress_bar(ProgressProxy progress_proxy);
        void set_expected_size(std::size_t size);

        const std::string& name() const;

        void init_curl_target(const std::string& url);

        bool resource_exists();
        bool perform();
        CURL* handle();

        curl_off_t get_speed();

        template <class C>
        inline void set_finalize_callback(bool (C::*cb)(), C* data)
        {
            m_finalize_callback = std::bind(cb, data);
        }

        inline void set_ignore_failure(bool yes)
        {
            m_ignore_failure = yes;
        }

        inline bool ignore_failure() const
        {
            return m_ignore_failure;
        }

        void set_result(CURLcode r);
        bool finalize();

        bool can_retry();
        CURL* retry();

        CURLcode result;
        bool failed = false;
        int http_status = 10000;
        curl_off_t downloaded_size = 0;
        curl_off_t avg_speed = 0;
        std::string final_url;

        std::string etag, mod, cache_control;

    private:
        std::function<bool()> m_finalize_callback;

        std::string m_name, m_filename, m_url;

        // validation
        std::size_t m_expected_size = 0;

        std::chrono::steady_clock::time_point m_progress_throttle_time;

        // retry & backoff
        std::chrono::steady_clock::time_point m_next_retry;
        std::size_t m_retry_wait_seconds = Context::instance().retry_timeout;
        std::size_t m_retries = 0;

        CURL* m_handle;
        curl_slist* m_headers;

        bool m_has_progress_bar = false;
        bool m_ignore_failure = false;

        ProgressProxy m_progress_bar;

        char m_errbuf[CURL_ERROR_SIZE];
        std::ofstream m_file;

        static void init_curl_handle(CURL* handle, const std::string& url);
    };

    class MultiDownloadTarget
    {
    public:
        MultiDownloadTarget();
        ~MultiDownloadTarget();

        void add(DownloadTarget* target);
        bool check_msgs(bool failfast);
        bool download(bool failfast);

    private:
        std::vector<DownloadTarget*> m_targets;
        std::vector<DownloadTarget*> m_retry_targets;
        CURLM* m_handle;
    };

}  // namespace mamba

#endif  // MAMBA_FETCH_HPP
