#include <string_view>
#include <thread>

#include "context.hpp"
#include "fetch.hpp"
#include "util.hpp"

namespace mamba
{
    /*********************************
     * DownloadTarget implementation *
     *********************************/

    DownloadTarget::DownloadTarget(const std::string& name, const std::string& url, const std::string& filename)
        : m_name(name)
        , m_filename(filename)
        , m_url(url)
    {
        m_file = std::ofstream(filename, std::ios::binary);

        m_handle = curl_easy_init();

        init_curl_target(m_url);
    }

    void DownloadTarget::init_curl_target(const std::string& url)
    {
        curl_easy_setopt(m_handle, CURLOPT_URL, url.c_str());

        curl_easy_setopt(m_handle, CURLOPT_HEADERFUNCTION, &DownloadTarget::header_callback);
        curl_easy_setopt(m_handle, CURLOPT_HEADERDATA, this);

        curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION, &DownloadTarget::write_callback);
        curl_easy_setopt(m_handle, CURLOPT_WRITEDATA, this);

        m_headers = nullptr;
        if (ends_with(url, ".json"))
        {
            curl_easy_setopt(m_handle, CURLOPT_ACCEPT_ENCODING, "gzip, deflate, compress, identity");
            m_headers = curl_slist_append(m_headers, "Content-Type: application/json");
        }
        curl_easy_setopt(m_handle, CURLOPT_HTTPHEADER, m_headers);
        curl_easy_setopt(m_handle, CURLOPT_VERBOSE, Context::instance().verbosity >= 2);

        std::string& ssl_verify = Context::instance().ssl_verify;

        if (!ssl_verify.size() && std::getenv("REQUESTS_CA_BUNDLE") != nullptr)
        {
            ssl_verify = std::getenv("REQUESTS_CA_BUNDLE");
        }

        if (ssl_verify.size())
        {
            if (ssl_verify == "<false>")
            {
                curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYPEER, 0L);
                curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYHOST, 0L);
            }
            else
            {
                if (!fs::exists(ssl_verify))
                {
                    throw std::runtime_error("ssl_verify does not contain a valid file path.");
                }
                else
                {
                    curl_easy_setopt(m_handle, CURLOPT_CAINFO, ssl_verify.c_str());
                }
            }
        }
    }

    bool DownloadTarget::can_retry()
    {
        return m_retries < Context::instance().max_retries;
    }

    CURL* DownloadTarget::retry()
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= m_next_retry)
        {
            init_curl_target(m_url);
            if (m_has_progress_bar)
            {
                curl_easy_setopt(m_handle, CURLOPT_XFERINFOFUNCTION, &DownloadTarget::progress_callback);
                curl_easy_setopt(m_handle, CURLOPT_XFERINFODATA, this);
            }
            m_retry_wait_seconds = m_retry_wait_seconds * Context::instance().retry_backoff;
            m_next_retry = now + std::chrono::seconds(m_retry_wait_seconds);
            m_retries++;
            return m_handle;
        }
        else
        {
            return nullptr;
        }
    }

    DownloadTarget::~DownloadTarget()
    {
        curl_easy_cleanup(m_handle);
        curl_slist_free_all(m_headers);
    }

    size_t DownloadTarget::write_callback(char *ptr, size_t size, size_t nmemb, void *self)
    {
        auto* s = (DownloadTarget*)self;
        s->m_file.write(ptr, size * nmemb);
        return size * nmemb;
    }

    size_t DownloadTarget::header_callback(char *buffer, size_t size, size_t nitems, void *self)
    {
        auto* s = (DownloadTarget*)self;

        std::string_view header(buffer, size * nitems);
        auto colon_idx = header.find(':');
        if (colon_idx != std::string_view::npos)
        {
            std::string_view key, value;
            key = header.substr(0, colon_idx);
            colon_idx++;
            // remove spaces
            while (std::isspace(header[colon_idx]))
            {
                ++colon_idx;
            }
            // remove \r\n header ending
                
            value = header.substr(colon_idx, header.size() - colon_idx - 2);
            if (key == "ETag")
            {
                s->etag = value;
            }
            else if (key == "Cache-Control")
            {
                s->cache_control = value;
            }
            else if (key == "Last-Modified")
            {
                s->mod = value;
            }
        }
        return nitems * size;
    }

    int DownloadTarget::progress_callback(void*, curl_off_t total_to_download, curl_off_t now_downloaded, curl_off_t, curl_off_t)
    {
        if (Context::instance().quiet || Context::instance().json)
        {
            return 0;
        }

        auto now = std::chrono::steady_clock::now();
        if (now - m_progress_throttle_time < std::chrono::milliseconds(150))
        {
            return 0;
        }
        m_progress_throttle_time = now;

        if (total_to_download != 0 && now_downloaded == 0 && m_expected_size != 0)
        {
            now_downloaded = total_to_download;
            total_to_download = m_expected_size;
        }

        if ((total_to_download != 0 || m_expected_size != 0) && now_downloaded != 0)
        {
            double perc = double(now_downloaded) / double(total_to_download);
            std::stringstream postfix;
            postfix << std::setw(6);
            to_human_readable_filesize(postfix, now_downloaded);
            postfix << " / ";
            postfix << std::setw(6);
            to_human_readable_filesize(postfix, total_to_download);
            postfix << " (";
            postfix << std::setw(6);
            to_human_readable_filesize(postfix, get_speed(), 2);
            postfix << "/s)";
            m_progress_bar.set_progress(perc * 100.);
            m_progress_bar.set_postfix(postfix.str());
        }
        if (now_downloaded == 0 && total_to_download != 0)
        {
            std::stringstream postfix;
            to_human_readable_filesize(postfix, total_to_download);
            postfix << " / ?? (";
            to_human_readable_filesize(postfix, get_speed(), 2);
            postfix << "/s)";
            m_progress_bar.set_progress(-1);
            m_progress_bar.set_postfix(postfix.str());
        }
        return 0;
    }

    void DownloadTarget::set_mod_etag_headers(const nlohmann::json& mod_etag)
    {
        auto to_header = [](const std::string& key, const std::string& value) {
            return std::string(key + ": " + value);
        };

        if (mod_etag.find("_etag") != mod_etag.end()) 
        {
            m_headers = curl_slist_append(m_headers, to_header("If-None-Match", mod_etag["_etag"]).c_str());
        }
        if (mod_etag.find("_mod") != mod_etag.end()) 
        {
            m_headers = curl_slist_append(m_headers, to_header("If-Modified-Since", mod_etag["_mod"]).c_str());
        }
    }
    
    void DownloadTarget::set_progress_bar(ProgressProxy progress_proxy)
    {
        m_has_progress_bar = true;
        m_progress_bar = progress_proxy;
        curl_easy_setopt(m_handle, CURLOPT_XFERINFOFUNCTION, &DownloadTarget::progress_callback);
        curl_easy_setopt(m_handle, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(m_handle, CURLOPT_NOPROGRESS, 0L);
    }

    void DownloadTarget::set_expected_size(std::size_t size)
    {
        m_expected_size = size;
    }

    const std::string& DownloadTarget::name() const
    {
        return m_name;
    }

    bool DownloadTarget::perform()
    {
        CURLcode res = curl_easy_perform(m_handle);
        if (res != CURLE_OK)
        {
            throw std::runtime_error(curl_easy_strerror(res));
        }
        return m_finalize_callback ? m_finalize_callback() : true;
    }

    CURL* DownloadTarget::handle()
    {
        return m_handle;
    }

    curl_off_t DownloadTarget::get_speed()
    {
        curl_off_t speed;
        CURLcode res = curl_easy_getinfo(m_handle, CURLINFO_SPEED_DOWNLOAD_T, &speed);
        return res == CURLE_OK ? speed : 0;
    }

    void DownloadTarget::set_failed(const std::string& msg)
    {
        LOG_WARNING << "Transfer failed, retrying";
        m_next_retry = std::chrono::steady_clock::now() + std::chrono::seconds(m_retry_wait_seconds);
        m_progress_bar.set_progress(0);
        m_progress_bar.set_postfix(msg);
    }
        
    bool DownloadTarget::finalize()
    {
        char* effective_url = nullptr;
        curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &http_status);
        curl_easy_getinfo(m_handle, CURLINFO_EFFECTIVE_URL, &effective_url);
        curl_easy_getinfo(m_handle, CURLINFO_SIZE_DOWNLOAD_T, &downloaded_size);

        LOG_INFO << "Transfer finalized, status: " << http_status << " @ " << effective_url << " " << downloaded_size << " bytes";

        if (http_status >= 500 && can_retry())
        {
            // this request didn't work!
            std::stringstream msg;
            msg << "Failed (" << http_status << "), retry in " << m_retry_wait_seconds << "s";
            set_failed(msg.str());
            return false;
        }

        m_file.close();

        final_url = effective_url;
        if (m_finalize_callback)
        {
            return m_finalize_callback();
        }
        else
        {
            if (m_has_progress_bar)
            {
                m_progress_bar.mark_as_completed("Downloaded " + m_name);
            }
        }
        return true;
    }

    /**************************************
     * MultiDownloadTarget implementation *
     **************************************/

    MultiDownloadTarget::MultiDownloadTarget()
    {
        m_handle = curl_multi_init();
        curl_multi_setopt(m_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS,
                          Context::instance().max_parallel_downloads);
    }

    MultiDownloadTarget::~MultiDownloadTarget()
    {
        curl_multi_cleanup(m_handle);
    }

    void MultiDownloadTarget::add(DownloadTarget* target)
    {
        if (!target) return;
        CURLMcode code = curl_multi_add_handle(m_handle, target->handle());
        if (code != CURLM_CALL_MULTI_PERFORM)
        {
            if (code != CURLM_OK)
            {
                throw std::runtime_error(curl_multi_strerror(code));
            }
        }
        m_targets.push_back(target);
    }

    bool MultiDownloadTarget::check_msgs()
    {
        int msgs_in_queue;
        CURLMsg *msg;

        while ((msg = curl_multi_info_read(m_handle, &msgs_in_queue)))
        {
            // TODO maybe refactor so that `msg` is passed to current target?
            DownloadTarget* current_target = nullptr;
            for (const auto& target : m_targets)
            {
                if (target->handle() == msg->easy_handle)
                {
                    current_target = target;
                    break;
                }
            }

            if (!current_target)
            {
                throw std::runtime_error("Could not find target associated with multi request");
            }

            if (msg->data.result != CURLE_OK)
            {
                if (current_target->can_retry())
                {
                    current_target->set_failed(curl_easy_strerror(msg->data.result));
                    curl_multi_remove_handle(m_handle, current_target->handle());
                    m_retry_targets.push_back(current_target);
                    continue;
                }
                char* effective_url = nullptr;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &effective_url);
                std::stringstream err;
                err << "Download error (" << msg->data.result << ") " <<
                        curl_easy_strerror(msg->data.result) << " [" << effective_url << "]";

                throw std::runtime_error(err.str());
            }

            if (msg->msg != CURLMSG_DONE)
            {
                // We are only interested in messages about finished transfers
                continue;
            }

            curl_multi_remove_handle(m_handle, current_target->handle());

            // flush file & finalize transfer
            if (!current_target->finalize())
            {
                // transfer did not work! can we retry?
                if (current_target->can_retry())
                {
                    LOG_WARNING << "Adding target to retry!";
                    m_retry_targets.push_back(current_target);
                }
                else
                {
                    throw std::runtime_error("Download failed.");
                }
            }
        }
        return true;
    }

    bool MultiDownloadTarget::download(bool failfast)
    {
        LOG_INFO << "Starting to download targets";

        int still_running, repeats = 0;
        const long max_wait_msecs = 1000;
        do
        {
            CURLMcode code = curl_multi_perform(m_handle, &still_running);                

            if(code != CURLM_OK)
            {
                throw std::runtime_error(curl_multi_strerror(code));
            }
            check_msgs();

            if (!m_retry_targets.empty())
            {
                auto it = m_retry_targets.begin();
                while (it != m_retry_targets.end())
                {
                    CURL* curl_handle = (*it)->retry();
                    if (curl_handle != nullptr)
                    {
                        curl_multi_add_handle(m_handle, curl_handle);
                        it = m_retry_targets.erase(it);
                        still_running = 1;
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            long curl_timeout = -1;
            code = curl_multi_timeout(m_handle, &curl_timeout);
            if (code != CURLM_OK)
            {
                throw std::runtime_error(curl_multi_strerror(code));
            }

            if (curl_timeout == 0) // No wait
                continue;

            if (curl_timeout < 0 || curl_timeout > max_wait_msecs) // Wait no more than 1s
                curl_timeout = max_wait_msecs;

            int numfds;
            code = curl_multi_wait(m_handle, NULL, 0, curl_timeout, &numfds);
            if (code != CURLM_OK)
            {
                throw std::runtime_error(curl_multi_strerror(code));
            }

            if(!numfds)
            {
                LOG_WARNING << "Nothing happend " << repeats;
                repeats++; // count number of repeated zero numfds
                if(repeats > 1)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            else
            {
                repeats = 0;
            }
        }
        while ((still_running || !m_retry_targets.empty()) && !Context::instance().sig_interrupt);

        if (Context::instance().sig_interrupt)
        {
            Console::instance().init_multi_progress();
            Console::print("Download interrupted");
            curl_multi_cleanup(m_handle);
            return false;
        }
        return true;
    }
}

