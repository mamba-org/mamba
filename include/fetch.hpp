#ifndef MAMBA_FETCH_HPP
#define MAMBA_FETCH_HPP

#include "nlohmann/json.hpp"

#include "thirdparty/indicators/dynamic_progress.hpp"
#include "thirdparty/indicators/progress_bar.hpp"

#include "output.hpp"
#include "validate.hpp"

extern "C"
{
    #include <stdio.h>
    #include <string.h>

    // unix-specific
    #ifndef _WIN32
    #include <sys/time.h>
    #include <unistd.h>
    #endif

    #include <curl/curl.h>
    #include <archive.h>
}

namespace mamba
{

    class DownloadTarget
    {
    public:

        DownloadTarget() = default;
        DownloadTarget(const std::string& name, const std::string& url, const std::string& filename);
        ~DownloadTarget();

        static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *self);
        static size_t header_callback(char *buffer, size_t size, size_t nitems, void *self);

        int progress_callback(void*, curl_off_t total_to_download, curl_off_t now_downloaded, curl_off_t, curl_off_t);
        void set_mod_etag_headers(const nlohmann::json& mod_etag);
        void set_progress_bar(ProgressProxy progress_proxy);
        void set_expected_size(std::size_t size);

        const std::string& name() const;

        bool perform();
        CURL* handle();
        curl_off_t get_speed();

        template <class C>
        void set_finalize_callback(int (C::*cb)(), C* data)
        {
            m_finalize_callback = std::bind(cb, data);
        }

        bool finalize();
        void validate();
        void set_sha256(const std::string& sha256);

        int http_status;
        std::string final_url;
        curl_off_t downloaded_size;

        std::string etag, mod, cache_control;

    private:

        std::function<int()> m_finalize_callback;

        std::string m_name, m_filename;

        // validation
        std::size_t m_expected_size = 0;
        std::string m_sha256;

        std::chrono::steady_clock::time_point m_progress_throttle_time;

        CURL* m_target;
        curl_slist* m_headers;

        bool m_has_progress_bar = false;
        ProgressProxy m_progress_bar;

        std::ofstream m_file;
    };

    class MultiDownloadTarget
    {
    public:

        MultiDownloadTarget();
        ~MultiDownloadTarget();

        void add(std::unique_ptr<DownloadTarget>& target);
        bool check_msgs();
        bool download(bool failfast);

    private:

        std::vector<DownloadTarget*> m_targets;
        CURLM* m_handle;
    };

}

#endif // MAMBA_FETCH_HPP
