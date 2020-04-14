#include <string>
#include "thirdparty/filesystem.hpp"
#include <regex>

#include "nlohmann/json.hpp"
#include "thirdparty/indicators/dynamic_progress.hpp"
#include "thirdparty/indicators/progress_bar.hpp"

#include "context.hpp"
#include "util.hpp"

extern "C" {
    #include <stdio.h>
    #include <string.h>

    /* somewhat unix-specific */
    #include <sys/time.h>
    #include <unistd.h>

    #include <curl/curl.h>
    #include <archive.h>
}

namespace fs = ghc::filesystem;

#define PREFIX_LENGTH 25

void to_human_readable_filesize(std::ostream& o, double bytes, std::size_t precision = 0)
{
    const char* sizes[] = { " B", "KB", "MB", "GB", "TB" };
    int order = 0;
    while (bytes >= 1024 && order < (5 - 1)) {
        order++;
        bytes = bytes / 1024;
    }
    o << std::fixed << std::setprecision(precision) << bytes << sizes[order];
}

namespace decompress
{
    bool raw(const std::string& in, const std::string& out)
    {
        int r;
        ssize_t size;

        LOG(INFO) << "Decompressing from " << in << " to " << out;

        struct archive *a = archive_read_new();
        archive_read_support_filter_bzip2(a);
        archive_read_support_format_raw(a);
        // TODO figure out good value for this
        const std::size_t BLOCKSIZE = 16384;
        r = archive_read_open_filename(a, in.c_str(), BLOCKSIZE);
        if (r != ARCHIVE_OK) {
            return false;
        }

        struct archive_entry *entry;
        std::ofstream out_file(out);
        char buff[BLOCKSIZE];
        std::size_t buffsize = BLOCKSIZE;
        r = archive_read_next_header(a, &entry);
        if (r != ARCHIVE_OK) {
            return false;
        }

        while (true)
        {
            size = archive_read_data(a, &buff, buffsize);
            if (size < ARCHIVE_OK)
            {
                throw std::runtime_error(std::string("Could not read archive: ") + archive_error_string(a));
            }
            if (size == 0)
            {
                break;
            }
            out_file.write(buff, size);
        }

        archive_read_free(a);
        return true;
    }
}

namespace mamba
{

    class DownloadTarget
    {
    public:
        static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *self)
        {
            auto* s = (DownloadTarget*)self;
            s->m_file.write(ptr, size * nmemb);
            return size * nmemb;
        }

        DownloadTarget() = default;

        DownloadTarget(const std::string& url, const std::string& filename)
        {
            m_file = std::ofstream(filename, std::ios::binary);

            m_target = curl_easy_init();

            curl_easy_setopt(m_target, CURLOPT_URL, url.c_str());

            curl_easy_setopt(m_target, CURLOPT_HEADERFUNCTION, &DownloadTarget::header_callback);
            curl_easy_setopt(m_target, CURLOPT_HEADERDATA, this);

            curl_easy_setopt(m_target, CURLOPT_WRITEFUNCTION, &DownloadTarget::write_callback);
            curl_easy_setopt(m_target, CURLOPT_WRITEDATA, this);

            m_headers = nullptr;
            if (ends_with(url, ".json"))
            {
                curl_easy_setopt(m_target, CURLOPT_ACCEPT_ENCODING, "gzip, deflate, compress, identity");
                m_headers = curl_slist_append(m_headers, "Content-Type: application/json");
            }
            curl_easy_setopt(m_target, CURLOPT_HTTPHEADER, m_headers);
            curl_easy_setopt(m_target, CURLOPT_VERBOSE, Context::instance().verbosity != 0);
        }

        void set_mod_etag_headers(const nlohmann::json& mod_etag)
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

        void set_progress_callback(int (*cb)(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t), void* data)
        {
            curl_easy_setopt(m_target, CURLOPT_XFERINFOFUNCTION, cb);
            curl_easy_setopt(m_target, CURLOPT_XFERINFODATA, data);
            curl_easy_setopt(m_target, CURLOPT_NOPROGRESS, 0L);
        }

        static size_t header_callback(char *buffer, size_t size, size_t nitems, void *self)
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

        bool perform()
        {
            return curl_easy_perform(m_target);
        }

        CURL* handle()
        {
            return m_target;
        }

        curl_off_t get_speed()
        {
            curl_off_t speed;
            CURLcode res = curl_easy_getinfo(m_target, CURLINFO_SPEED_DOWNLOAD_T, &speed);
            if (!res)
            {
                return speed;
            }
            return 0;
        }

        bool finalize()
        {
            m_file.flush();
        }

        ~DownloadTarget()
        {
            curl_easy_cleanup(m_target);
            curl_slist_free_all(m_headers);
        }

        std::string etag, mod, cache_control;

        CURL* m_target;
        curl_slist* m_headers;
        std::ofstream m_file;
    };

    class MSubdirData
    {
    public:
        MSubdirData(const std::string& name, const std::string& url, const std::string& repodata_fn) :
            m_name(name),
            m_url(url),
            m_json_fn(repodata_fn),
            m_solv_fn(repodata_fn.substr(0, repodata_fn.size() - 4) + "solv"),
            m_loaded(false),
            m_download_complete(false)
        {
        }

        // TODO return seconds as double
        double check_cache(const fs::path& cache_file)
        {
            try {
                auto last_write = fs::last_write_time(cache_file);
                auto cftime = decltype(last_write)::clock::now();
                auto tdiff = cftime - last_write;
                auto as_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(tdiff);
                return as_seconds.count();
            }
            catch(...)
            {
                // could not open the file...
                return -1;
            }
        }

        bool loaded()
        {
            return m_loaded;
        }

        bool forbid_cache()
        {
            return starts_with(m_url, "file://");
        }

        bool load(/*Handle& handle*/)
        {
            double cache_age = check_cache(m_json_fn);
            nlohmann::json mod_etag_headers;
            if (cache_age != -1)
            {
                LOG(INFO) << "Found valid cache file.";
                mod_etag_headers = read_mod_and_etag();
                if (mod_etag_headers.size() != 0) {

                    int max_age = 0;
                    if (Context::instance().local_repodata_ttl > 1)
                    {
                        max_age = Context::instance().local_repodata_ttl;
                    }
                    else if (Context::instance().local_repodata_ttl == 1)
                    {
                        // TODO error handling if _cache_control key does not exist!
                        auto el = mod_etag_headers.value("_cache_control", std::string(""));
                        max_age = get_cache_control_max_age(el);
                    }
                    if ((max_age > cache_age || Context::instance().offline) && !forbid_cache())
                    {
                        // cache valid!
                        LOG(INFO) << "Using cache " << m_url << " age in seconds: " << cache_age << " / " << max_age;
                        OUTPUT(std::left << std::setw(PREFIX_LENGTH) << m_name << "Using cache");
                        m_loaded = true;
                        m_json_cache_valid = true;

                        // check solv cache
                        double solv_age = check_cache(m_solv_fn);
                        if (solv_age != -1 && solv_age <= cache_age)
                        {
                            LOG(INFO) << "Also using .solv cache file";
                            m_solv_cache_valid = true;
                        }

                        return true;
                    }
                }
                else
                {
                    LOG(WARNING) << "Could not determine mod / etag headers.";
                }
                create_target(mod_etag_headers);
            }
            else
            {
                LOG(INFO) << "No cache found " << m_url;
                create_target(mod_etag_headers);
            }
            return true;
        }

        std::string cache_path() const
        {
            // TODO invalidate solv cache on version updates!!
            if (m_json_cache_valid && m_solv_cache_valid)
            {
                return m_solv_fn;
            }
            else if (m_json_cache_valid)
            {
                return m_json_fn;
            }
            throw std::runtime_error("Cache not loaded!");
        }

        std::unique_ptr<DownloadTarget>& target()
        {
            return m_target;
        }

        const std::string& name() const
        {
            return m_name;
        }

        void set_progress_bar(std::size_t idx, indicators::DynamicProgress<indicators::ProgressBar>* ptr)
        {
            m_multi_progress_idx = idx;
            p_multi_progress = ptr;
        }

        int finalize_transfer(int status)
        {
            auto& mp = *(p_multi_progress);

            LOG(WARNING) << "HTTP response code: " << status;
            if (status == 304)
            {
                // cache still valid
                double cache_age = check_cache(m_json_fn);
                double solv_age = check_cache(m_solv_fn);

                using fs_time_t = decltype(fs::last_write_time(fs::path()));
                fs::last_write_time(m_json_fn, fs_time_t::clock::now());

                if(solv_age != -1 && solv_age <= cache_age) {
                    fs::last_write_time(m_solv_fn, fs_time_t::clock::now());
                    m_solv_cache_valid = true;
                }

                if (!Context::instance().quiet)
                {
                    mp[m_multi_progress_idx].set_option(indicators::option::PostfixText{"No change"});
                    mp[m_multi_progress_idx].set_progress(100);
                    mp[m_multi_progress_idx].mark_as_completed();
                }

                m_json_cache_valid = true;
                m_loaded = true;

                return 0;
            }

            LOG(WARNING) << "Finalized transfer: " << m_url;

            nlohmann::json prepend_header;

            prepend_header["_url"] = m_url;
            prepend_header["_etag"] = m_target->etag;
            prepend_header["_mod"] = m_target->mod;
            prepend_header["_cache_control"] = m_target->cache_control;

            LOG(WARNING) << "Opening: " << m_json_fn;
            std::ofstream final_file(m_json_fn);
            // TODO make sure that cache directory exists!
            if (!final_file.is_open())
            {
                throw std::runtime_error("Could not open file.");
            }

            if (ends_with(m_url, ".bz2"))
            {
                if (!Context::instance().quiet)
                {
                    mp[m_multi_progress_idx].set_option(indicators::option::PostfixText{"Decomp..."});
                }
                m_temp_name = decompress();
            }

            if (!Context::instance().quiet)
            {
                mp[m_multi_progress_idx].set_option(indicators::option::PostfixText{"Finalizing..."});
            }

            std::ifstream temp_file(m_temp_name);
            std::stringstream temp_json;
            temp_json << prepend_header.dump();
            // replace `}` with `,`
            temp_json.seekp(-1, temp_json.cur); temp_json << ',';
            final_file << temp_json.str();
            temp_file.seekg(1);
            std::copy(
                std::istreambuf_iterator<char>(temp_file),
                std::istreambuf_iterator<char>(),
                std::ostreambuf_iterator<char>(final_file)
            );

            if (!Context::instance().quiet)
            {
                mp[m_multi_progress_idx].set_option(indicators::option::PostfixText{"Done"});
                mp[m_multi_progress_idx].set_progress(100);
                mp[m_multi_progress_idx].mark_as_completed();
            }

            m_json_cache_valid = true;
            m_loaded = true;
            return 0;
        }


    private:

        std::string decompress()
        {
            LOG(INFO) << "Decompressing metadata";
            auto json_temp_name = std::tmpnam(nullptr);
            bool result = decompress::raw(m_temp_name, json_temp_name);
            if (!result)
            {
                LOG(WARNING) << "Could not decompress " << m_temp_name;
            }
            return json_temp_name;
        }

        static int progress_callback(void *self, curl_off_t total_to_download, curl_off_t now_downloaded, curl_off_t, curl_off_t)
        {
            auto* s = (MSubdirData*)self;
            if (Context::instance().quiet)
            {
                return 0;
            }

            auto& mp = (*(s->p_multi_progress));

            if (!s->m_download_complete && total_to_download != 0)
            {
                double perc = double(now_downloaded) / double(total_to_download);
                // std::stringstream postfix;
                // to_human_readable_filesize(postfix, now_downloaded);
                // postfix << " / ";
                // to_human_readable_filesize(postfix, total_to_download);
                // postfix << " (";
                // to_human_readable_filesize(postfix, s->target()->get_speed(), 2);
                // postfix << "/s)";
                // mp[s->m_multi_progress_idx].set_option(indicators::option::PostfixText{postfix.str()});
                mp[s->m_multi_progress_idx].set_progress(perc * 100.);
                if (std::ceil(perc * 100.) >= 100)
                {
                    mp[s->m_multi_progress_idx].mark_as_completed();
                    s->m_download_complete = true;
                }
            }
            if (total_to_download == 0 && now_downloaded != 0)
            {
                std::stringstream postfix;
                to_human_readable_filesize(postfix, now_downloaded);
                postfix << " / ?? (";
                to_human_readable_filesize(postfix, s->target()->get_speed(), 2);
                postfix << "/s)";
                mp[s->m_multi_progress_idx].set_option(indicators::option::PostfixText{postfix.str()});
            }
            return 0;
        }

        void create_target(nlohmann::json& mod_etag /*, Handle& handle*/)
        {
            m_temp_name = std::tmpnam(nullptr);
            m_target = std::make_unique<DownloadTarget>(m_url, m_temp_name);
            m_target->set_progress_callback(&MSubdirData::progress_callback, this);
            m_target->set_mod_etag_headers(mod_etag);
        }

        std::size_t get_cache_control_max_age(const std::string& val)
        {
            std::regex max_age_re("max-age=(\\d+)");
            std::smatch max_age_match;
            bool matches = std::regex_search(val, max_age_match, max_age_re);
            if (!matches) return 0;
            return std::stoi(max_age_match[1]);
        }

        nlohmann::json read_mod_and_etag()
        {
            // parse json at the beginning of the stream such as
            // {"_url": "https://conda.anaconda.org/conda-forge/linux-64", 
            // "_etag": "W/\"6092e6a2b6cec6ea5aade4e177c3edda-8\"", 
            // "_mod": "Sat, 04 Apr 2020 03:29:49 GMT",
            // "_cache_control": "public, max-age=1200"

            auto extract_subjson = [](std::ifstream& s)
            {
                char next;
                std::string result;
                bool escaped = false;
                int i = 0, N = 4;
                while (s.get(next)) {
                    if (next == '"')
                    {
                        if (!escaped)
                        {
                            i++;
                        }
                        else
                        {
                            escaped = false;
                        }
                        // 4 keys == 4 ticks
                        if (i == 4 * N)
                        {
                            return result + "\"}";
                        }
                    } 
                    else if (next == '\\')
                    {
                        escaped = true;
                    }
                    result.push_back(next);
                }
                return std::string();
            };

            std::ifstream in_file(m_json_fn);
            auto json = extract_subjson(in_file);
            nlohmann::json result;
            try {
                result = nlohmann::json::parse(json);
                return result;
            }
            catch(...)
            {
                LOG(WARNING) << "Could not parse mod / etag header!";
                return nlohmann::json();
            }
        }

        std::unique_ptr<DownloadTarget> m_target;

        bool m_json_cache_valid = false;
        bool m_solv_cache_valid = false;

        std::ofstream out_file;

        indicators::DynamicProgress<indicators::ProgressBar>* p_multi_progress;
        std::size_t m_multi_progress_idx;

        bool m_loaded, m_download_complete;
        std::string m_url;
        std::string m_name;
        std::string m_json_fn;
        std::string m_solv_fn;
        std::string m_temp_name;
    };

    class MultiDownloadTarget
    {
    public:
        MultiDownloadTarget()
        {
            m_handle = curl_multi_init();
        }

        ~MultiDownloadTarget()
        {
            curl_multi_cleanup(m_handle);
        }

        void add(MSubdirData& subdirdata)
        {
            if (subdirdata.loaded())
            {
                LOG(WARNING) << "Adding cached or previously loaded subdirdata, ignoring";
                return;
            }
            if (subdirdata.target() == nullptr)
            {
                LOG(WARNING) << "Subdirdata load not called, ignoring";
                return;
            }
            CURLMcode code = curl_multi_add_handle(m_handle, subdirdata.target()->handle());
            if(code != CURLM_CALL_MULTI_PERFORM)
            {
                if(code != CURLM_OK)
                {
                    throw std::runtime_error(curl_multi_strerror(code));
                }
            }

            if (subdirdata.target() != nullptr)
            {
                std::string prefix = subdirdata.name();
                if (prefix.size() < (PREFIX_LENGTH - 1)) {
                    prefix += std::string(PREFIX_LENGTH - prefix.size(), ' ');
                }
                else if (prefix.size() > (PREFIX_LENGTH - 1))
                {
                    prefix = prefix.substr(0, (PREFIX_LENGTH - 1)) + std::string(" ");
                }

                m_progress_bars.push_back(std::make_unique<indicators::ProgressBar>(
                    indicators::option::BarWidth{15},
                    indicators::option::ForegroundColor{indicators::Color::unspecified},
                    indicators::option::ShowElapsedTime{true},
                    indicators::option::ShowRemainingTime{false},
                    indicators::option::MaxPostfixTextLen{36},
                    indicators::option::PrefixText(prefix)
                ));

                auto idx = m_multi_progress.push_back(*m_progress_bars[m_progress_bars.size() - 1].get());
                subdirdata.set_progress_bar(idx, &m_multi_progress);
                m_subdir_data.push_back(&subdirdata);
            }
        }

        bool check_msgs()
        {
            int msgs_in_queue;
            CURLMsg *msg;

            while ((msg = curl_multi_info_read(m_handle, &msgs_in_queue))) {

                if (msg->data.result != CURLE_OK) {
                    char* effective_url = nullptr;
                    curl_easy_getinfo(msg->easy_handle,
                                      CURLINFO_EFFECTIVE_URL,
                                      &effective_url);
                    std::stringstream err;
                    err << "Download error (" << msg->data.result << ") " <<
                           curl_easy_strerror(msg->data.result) << "[" << effective_url << "]";

                    throw std::runtime_error(err.str());
                }
                if (msg->msg != CURLMSG_DONE) {
                    // We are only interested in messages about finished transfers
                    continue;
                }

                MSubdirData* subdir_data = nullptr;
                for (const auto& sd : m_subdir_data)
                {
                    if (sd->target()->handle() == msg->easy_handle)
                    {
                        subdir_data = sd;
                        break;
                    }
                }
                subdir_data->target()->finalize();

                if (!subdir_data)
                {
                    throw std::runtime_error("transfer failed");   
                }

                int response_code;
                char* effective_url = nullptr;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
                curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &effective_url);

                LOG(INFO) << "response_code " << response_code << " @ " << effective_url;
                subdir_data->finalize_transfer(response_code);
            }
            return true;
        }

        bool download(bool failfast)
        {
            m_multi_progress.set_option(indicators::option::HideBarWhenComplete{false});
            for (auto& bar : m_progress_bars)
            {
                bar->set_progress(0);
            }
            LOG(INFO) << "Starting to download targets";

            int still_running, repeats = 0;
            const long max_wait_msecs = 400;
            int i = 0;
            do
            {
                CURLMcode code = curl_multi_perform(m_handle, &still_running);                

                if(code != CURLM_OK)
                {
                    throw std::runtime_error(curl_multi_strerror(code));
                }
                check_msgs();

                int numfds;
                code = curl_multi_wait(m_handle, NULL, 0, max_wait_msecs, &numfds);

                if (code != CURLM_OK)
                {
                    throw std::runtime_error(curl_multi_strerror(code));
                }

                if(!numfds)
                {
                    repeats++; // count number of repeated zero numfds
                    if(repeats > 1)
                    {
                    // wait 100 ms
                    #ifdef _WIN32
                        Sleep(100)
                    #else
                        // Portable sleep for platforms other than Windows.
                        struct timeval wait = { 0, 100 * 1000 };
                        (void) select(0, NULL, NULL, NULL, &wait);
                    #endif
                    }
                }
                else
                {
                    repeats = 0;
                }
            } while (still_running);

            return true;
        }

        std::vector<std::unique_ptr<indicators::ProgressBar>> m_progress_bars;
        indicators::DynamicProgress<indicators::ProgressBar> m_multi_progress;

        std::vector<MSubdirData*> m_subdir_data;
        CURLM* m_handle;
    };
}