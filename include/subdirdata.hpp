#include <string>
#include "thirdparty/filesystem.hpp"
#include <regex>

#include "nlohmann/json.hpp"
#include "thirdparty/indicators/dynamic_progress.hpp"
#include "thirdparty/indicators/progress_bar.hpp"

#include "context.hpp"
#include "util.hpp"
#include "fetch.hpp"
#include "output.hpp"

namespace fs = ghc::filesystem;

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
                        std::string prefix = m_name;
                        prefix.resize(PREFIX_LENGTH - 1, ' ');
                        Output::print() << prefix << "Using cache\n";

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

        int finalize_transfer()
        {
            LOG(WARNING) << "HTTP response code: " << m_target->http_status;
            if (m_target->http_status == 304)
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

                m_progress_bar.set_option(indicators::option::PostfixText{"No change"});
                m_progress_bar.set_progress(100);
                m_progress_bar.mark_as_completed();

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
                m_progress_bar.set_option(indicators::option::PostfixText{"Decomp..."});
                m_temp_name = decompress();
            }

            m_progress_bar.set_option(indicators::option::PostfixText{"Finalizing..."});

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

            m_progress_bar.set_option(indicators::option::PostfixText{"Done"});
            m_progress_bar.set_progress(100);
            m_progress_bar.mark_as_completed();

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

            if (!s->m_download_complete && total_to_download != 0)
            {
                double perc = double(now_downloaded) / double(total_to_download);
                std::stringstream postfix;
                to_human_readable_filesize(postfix, now_downloaded);
                postfix << " / ";
                to_human_readable_filesize(postfix, total_to_download);
                postfix << " (";
                to_human_readable_filesize(postfix, s->target()->get_speed(), 2);
                postfix << "/s)";
                s->m_progress_bar.set_option(indicators::option::PostfixText{postfix.str()});
                s->m_progress_bar.set_progress(perc * 100.);
                if (std::ceil(perc * 100.) >= 100)
                {
                    s->m_progress_bar.mark_as_completed();
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
                s->m_progress_bar.set_option(indicators::option::PostfixText{postfix.str()});
            }
            return 0;
        }

        void create_target(nlohmann::json& mod_etag)
        {
            m_temp_name = std::tmpnam(nullptr);
            m_progress_bar = Output::instance().add_progress_bar(m_name);
            m_target = std::make_unique<DownloadTarget>(m_name, m_url, m_temp_name);
            m_target->set_progress_callback(&MSubdirData::progress_callback, this);
            m_target->set_finalize_callback(&MSubdirData::finalize_transfer, this);
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

        Output::ProgressProxy m_progress_bar;

        bool m_loaded, m_download_complete;
        std::string m_url;
        std::string m_name;
        std::string m_json_fn;
        std::string m_solv_fn;
        std::string m_temp_name;
    };
}