#include <string>
#include <filesystem>
#include <regex>

#include "nlohmann/json.hpp"
#include "thirdparty/indicators/dynamic_progress.hpp"
#include "thirdparty/indicators/progress_bar.hpp"

#include "context.hpp"
#include "util.hpp"

extern "C" {
    #include <glib.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <librepo/librepo.h>
    #include <archive.h>
}

void to_human_readable_filesize(std::ostream& o, double bytes)
{
    const char* sizes[] = { " B", "KB", "MB", "GB", "TB" };
    int order = 0;
    while (bytes >= 1024 && order < 5 - 1) {
        order++;
        bytes = bytes / 1024;
    }
    o << int(bytes) << sizes[order];
}

namespace fs = std::filesystem;

namespace decompress
{
    bool raw(const std::string& in, const std::string& out)
    {
        int r;
        ssize_t size;

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
          if (size < 0) {
              /* ERROR */
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

    class Handle
    {
    public:
        Handle() {
            m_handle = lr_handle_init();
        }

        ~Handle() {
            lr_handle_free(m_handle);
        }

        operator LrHandle*() {
            return m_handle;
        }

        // Result perform() {
        //     LrResult* result = lr_result_init();
        //     GError* tmp_err = NULL;
        //     std::cout << "Downloading! " << std::endl;
        //     lr_handle_perform(m_handle, result, &tmp_err);
        //     // g_error_free(tmp_err);
        //     char *destdir;
        //     lr_handle_getinfo(m_handle, NULL, LRI_DESTDIR, &destdir);

        //     if (result) {
        //         printf("Download successful (Destination dir: %s)\n", destdir);
        //     } else {
        //         fprintf(stderr, "Error encountered: %s\n", tmp_err->message);
        //         g_error_free(tmp_err);
        //         // rc = EXIT_FAILURE;
        //     }

        //     return result;
        // }

        LrHandle* m_handle;
    };

    class MSubdirData
    {
    public:
        MSubdirData(const std::string& name, const std::string& url, const std::string& repodata_fn) :
            m_name(name),
            m_url(url),
            m_repodata_fn(repodata_fn),
            m_loaded(false),
            m_download_complete(false),
            m_target(nullptr)
        {
        }

        ptrdiff_t check_cache()
        {
            try {
                auto last_write = fs::last_write_time(m_repodata_fn);
                auto cftime = decltype(last_write)::clock::now();
                auto tdiff = cftime - last_write;
                auto as_seconds = std::chrono::duration_cast<std::chrono::seconds>(tdiff);
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
            ptrdiff_t cache_age = check_cache();
            nlohmann::json mod_etag_headers;
            if (check_cache() != -1)
            {
                LOG(INFO) << "Found valid cache file.";

                mod_etag_headers = read_mod_and_etag();
                if (mod_etag_headers.size() != 0) {
                    std::cout << mod_etag_headers << std::endl;

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
                        m_loaded = true;
                        return true;
                    }
                }
                else
                {
                    LOG(WARNING) << "Could not determine mod / etag headers.";
                }

            }
            else
            {
                // now we need to download the file
                LOG(INFO) << "Local cache timed out for " << m_url;
                create_target(mod_etag_headers); //, handle);
            }

            // raw_repodata_str = fetch_repodata_remote_request(self.url_w_credentials,
            //                                                  mod_etag_headers.get('_etag'),
            //                                                  mod_etag_headers.get('_mod'),
            //                                                  repodata_fn=self.repodata_fn)
            // if not raw_repodata_str and self.repodata_fn != REPODATA_FN:
            //     raise UnavailableInvalidChannel(self.url_w_repodata_fn, 404)
            return true;
        }

        LrDownloadTarget* target() const
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

        static int finalize_transfer(void* self, LrTransferStatus status, const char* msg)
        {
            auto* s = (MSubdirData*)self;
            auto& mp = (*(s->p_multi_progress));

            LOG(INFO) << "Finalized transfer: " << s->m_url;

            nlohmann::json prepend_header;

            // saved_fields = {'_url': url}
            // add_http_value_to_dict(resp, 'Etag', saved_fields, '_etag')
            // add_http_value_to_dict(resp, 'Last-Modified', saved_fields, '_mod')
            // add_http_value_to_dict(resp, 'Cache-Control', saved_fields, '_cache_control')

            prepend_header["_url"] = s->m_url;

            LOG(INFO) << "Opening: " << s->m_repodata_fn;
            std::ofstream final_file(s->m_repodata_fn);
            // TODO make sure that cache directory exists!
            if (!final_file.is_open())
            {
                throw std::runtime_error("Could not open file.");
            }

            if (ends_with(s->m_url, ".bz2"))
            {
                mp[s->m_multi_progress_idx].set_option(indicators::option::PostfixText{"Decomp..."});
                s->m_temp_name = s->decompress();
            }

            mp[s->m_multi_progress_idx].set_option(indicators::option::PostfixText{"Finalizing..."});

            std::ifstream temp_file(s->m_temp_name);
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
            mp[s->m_multi_progress_idx].set_option(indicators::option::PostfixText{"Done"});
            mp[s->m_multi_progress_idx].set_progress(100);
            mp[s->m_multi_progress_idx].mark_as_completed();

            return 0;
        }

        static int progress_callback(void *self, double total_to_download, double now_downloaded)
        {
            auto* s = (MSubdirData*)self;
            auto& mp = (*(s->p_multi_progress));

            if (!s->m_download_complete && total_to_download != 0)
            {
                if (total_to_download != 0)
                {
                    std::stringstream postfix;
                    to_human_readable_filesize(postfix, now_downloaded);
                    postfix << " / ";
                    to_human_readable_filesize(postfix, total_to_download);
                    mp[s->m_multi_progress_idx].set_option(indicators::option::PostfixText{postfix.str()});
                    mp[s->m_multi_progress_idx].set_progress((now_downloaded / total_to_download) * 100);
                }
                if (int(now_downloaded / total_to_download) * 100 >= 100)
                {
                    // mp[s->m_multi_progress_idx].mark_as_completed();
                    s->m_download_complete = true;
                }
            }
            return 0;
        }

        void create_target(nlohmann::json& mod_etag /*, Handle& handle*/)
        {
            m_temp_name = std::tmpnam(nullptr);
            m_target = lr_downloadtarget_new(
                // handle,           // ptr to handle
                nullptr,           // ptr to handle
                m_url.c_str(),    // url (absolute, or relative)
                nullptr,          // base url
                -1,               // file descriptor (opened)
                m_temp_name.c_str(), // filename
                nullptr,          // possible checksums
                0,                // expected size
                true,             // resume
                &MSubdirData::progress_callback,// progress cb
                this, // cb data
                // reinterpret_cast<int(void*, LrTransferStatus, const char*)>(&MSubdirData::finalize_transfer),     // end cb
                &MSubdirData::finalize_transfer,     // end cb
                nullptr,          // mirror failure cb
                nullptr,          // user data
                0,                // byte range start (download only range)
                0,                // byte range end
                nullptr,          // range string (overrides start / end, usage unclear)
                false,            // no_cache (tell proxy server that we want fresh data)
                false             // is zchunk
            );

            // auto to_header = [](const std::string& key, const std::string& value) {
            //     return std::string(key + ": " + value);
            // };

            // struct curl_slist *headers = nullptr;
            // if (mod_etag.find("_etag") != mod_etag.end()) 
            // {
            //     headers = curl_slist_append(headers, to_header("If-None-Match", mod_etag["_etag"]).c_str());
            // }
            // if (mod_etag.find("_mod") != mod_etag.end()) 
            // {
            //     headers = curl_slist_append(headers, to_header("If-Modified-Since", mod_etag["_mod"]).c_str());
            // }

            // headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate, compress, identity");
            // headers = curl_slist_append(headers, "Content-Type: application/json");
            // m_target->curl_rqheaders = headers;
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
            // {"_url": "https://conda.anaconda.org/conda-forge/linux-64", "_etag": "W/\"6092e6a2b6cec6ea5aade4e177c3edda-8\"", "_mod": "Sat, 04 Apr 2020 03:29:49 GMT", "_cache_control": "public, max-age=1200", 
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

            std::ifstream in_file(m_repodata_fn);
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

        LrDownloadTarget* m_target;

        indicators::DynamicProgress<indicators::ProgressBar>* p_multi_progress;
        std::size_t m_multi_progress_idx;
        bool m_loaded, m_download_complete;
        std::string m_url;
        std::string m_name;
        std::string m_repodata_fn;
        std::string m_temp_name;
    };

    class DownloadTargetList
    {
    public:

        DownloadTargetList()
        {
        }

        void append(MSubdirData& subdirdata)
        {
            const std::size_t PREFIX_LENGTH = 25;
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
                    indicators::option::BarWidth{25},
                    indicators::option::ForegroundColor{indicators::Color::unspecified},
                    indicators::option::ShowElapsedTime{true},
                    indicators::option::ShowRemainingTime{false},
                    indicators::option::PrefixText(prefix)
                ));
                auto idx = m_multi_progress.push_back(*m_progress_bars[m_progress_bars.size() - 1].get());
                subdirdata.set_progress_bar(idx, &m_multi_progress);

                m_download_targets = g_slist_append(m_download_targets, subdirdata.target());
            }
        }

        bool download(bool failfast)
        {
            std::cout << "Downloading metadata\n\n";
            m_multi_progress.set_option(indicators::option::HideBarWhenComplete{false});
            for (auto& bar : m_progress_bars)
            {
                bar->set_progress(0);
            }
            LOG(INFO) << "Starting to download targets";
            if (m_download_targets == nullptr)
            {
                LOG(WARNING) << "Nothing to download";
                return true;
            }
            GError* tmp_err = NULL;
            bool result = lr_download(m_download_targets, failfast, &tmp_err);

            if (result) {
                LOG(INFO) << "All downloads successful";
            } else {
                LOG(ERROR) << "Error encountered: " << tmp_err->message;
                g_error_free(tmp_err);
            }
            return result;
        }

        std::vector<std::unique_ptr<indicators::ProgressBar>> m_progress_bars;
        indicators::DynamicProgress<indicators::ProgressBar> m_multi_progress;
        GSList* m_download_targets = nullptr;
    };

}