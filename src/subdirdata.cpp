#include "subdirdata.hpp"
#include "output.hpp"

namespace decompress
{
    bool raw(const std::string& in, const std::string& out)
    {
        int r;
        std::ptrdiff_t size;

        LOG_INFO << "Decompressing from " << in << " to " << out;

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
    MSubdirData::MSubdirData(const std::string& name, const std::string& url, const std::string& repodata_fn)
        : m_name(name)
        , m_url(url)
        , m_json_fn(repodata_fn)
        , m_solv_fn(repodata_fn.substr(0, repodata_fn.size() - 4) + "solv")
        , m_loaded(false)
        , m_download_complete(false)
    {
    }

    fs::file_time_type::duration MSubdirData::check_cache(const fs::path& cache_file, const fs::file_time_type::clock::time_point& ref)
    {
        try {
            auto last_write = fs::last_write_time(cache_file);
            auto tdiff = ref - last_write;
            return tdiff;
            // auto as_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(tdiff);
            // return as_seconds.count();
        }
        catch(...)
        {
            // could not open the file...
            return fs::file_time_type::duration::max();
        }
    }

    bool MSubdirData::loaded()
    {
        return m_loaded;
    }

    bool MSubdirData::forbid_cache()
    {
        return starts_with(m_url, "file://");
    }

    bool MSubdirData::load()
    {
        auto now = fs::file_time_type::clock::now();
        auto cache_age = check_cache(m_json_fn, now);
        nlohmann::json mod_etag_headers;
        if (cache_age != fs::file_time_type::duration::max())
        {
            LOG_INFO << "Found valid cache file.";
            mod_etag_headers = read_mod_and_etag();
            if (mod_etag_headers.size() != 0)
            {
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

                auto cache_age_seconds = std::chrono::duration_cast<std::chrono::seconds>(cache_age).count();
                if ((max_age > cache_age_seconds || Context::instance().offline) && !forbid_cache())
                {
                    // cache valid!
                    LOG_INFO << "Using cache " << m_url << " age in seconds: " << cache_age_seconds << " / " << max_age;
                    std::string prefix = m_name;
                    prefix.resize(PREFIX_LENGTH - 1, ' ');
                    Console::stream() << prefix << " Using cache";

                    m_loaded = true;
                    m_json_cache_valid = true;

                    // check solv cache
                    auto solv_age = check_cache(m_solv_fn, now);
                    LOG_INFO << "Solv cache age in seconds: " << std::chrono::duration_cast<std::chrono::seconds>(solv_age).count();
                    if (solv_age != fs::file_time_type::duration::max() && solv_age.count() <= cache_age.count())
                    {
                        LOG_INFO << "Also using .solv cache file";
                        m_solv_cache_valid = true;
                    }
                    return true;
                }
            }
            else
            {
                LOG_WARNING << "Could not determine mod / etag headers.";
            }
            create_target(mod_etag_headers);
        }
        else
        {
            LOG_INFO << "No cache found " << m_url;
            if (!Context::instance().offline)
            {
                create_target(mod_etag_headers);
            }
        }
        return true;
    }

    std::string MSubdirData::cache_path() const
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

    DownloadTarget* MSubdirData::target()
    {
        return m_target.get();
    }

    const std::string& MSubdirData::name() const
    {
        return m_name;
    }

    bool MSubdirData::finalize_transfer()
    {
        LOG_WARNING << "HTTP response code: " << m_target->http_status;
        if (m_target->http_status == 200 || m_target->http_status == 304)
        {
            m_download_complete = true;
        }

        if (m_target->http_status == 304)
        {
            // cache still valid
            auto now = fs::file_time_type::clock::now();
            auto cache_age = check_cache(m_json_fn, now);
            auto solv_age = check_cache(m_solv_fn, now);

            using fs_time_t = decltype(fs::last_write_time(fs::path()));
            fs::last_write_time(m_json_fn, fs_time_t::clock::now());
            LOG_INFO << "Solv age: " << std::chrono::duration_cast<std::chrono::seconds>(solv_age).count() << ", JSON age: " << std::chrono::duration_cast<std::chrono::seconds>(cache_age).count();
            if(solv_age != fs::file_time_type::duration::max() && solv_age.count() <= cache_age.count())
            {
                fs::last_write_time(m_solv_fn, fs_time_t::clock::now());
                m_solv_cache_valid = true;
            }

            m_progress_bar.set_postfix("No change");
            m_progress_bar.set_progress(100);
            m_progress_bar.mark_as_completed();

            m_json_cache_valid = true;
            m_loaded = true;
            m_temp_file.reset(nullptr);
            return true;
        }

        if ((m_target->http_status == 404 || m_target->http_status == 403) && !ends_with(m_name, "/noarch"))
        {
            // we're ignoring a 404 on non-noarch channels like conda does
            LOG_INFO << "Unable to retrieve repodata (response: " << m_target->http_status << ") for " << m_url;
            m_progress_bar.set_postfix("404 Ignored");
            m_progress_bar.set_progress(100);
            m_progress_bar.mark_as_completed();
            m_loaded = false;
            return true;
        }

        LOG_INFO << "Finalized transfer: " << m_url;

        nlohmann::json prepend_header;

        prepend_header["_url"] = m_url;
        prepend_header["_etag"] = m_target->etag;
        prepend_header["_mod"] = m_target->mod;
        prepend_header["_cache_control"] = m_target->cache_control;

        LOG_WARNING << "Opening: " << m_json_fn;
        std::ofstream final_file(m_json_fn);
        // TODO make sure that cache directory exists!
        if (!final_file.is_open())
        {
            throw std::runtime_error("Could not open file.");
        }

        if (ends_with(m_url, ".bz2"))
        {
            m_progress_bar.set_postfix("Decomp...");
            decompress();
        }

        m_progress_bar.set_postfix("Finalizing...");

        std::ifstream temp_file(m_temp_file->path());
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

        m_progress_bar.set_postfix("Done");
        m_progress_bar.set_progress(100);
        m_progress_bar.mark_as_completed();

        m_json_cache_valid = true;
        m_loaded = true;

        temp_file.close();
        m_temp_file.reset(nullptr);

        return true;
    }

    bool MSubdirData::decompress()
    {
        LOG_INFO << "Decompressing metadata";
        auto json_temp_file = std::make_unique<TemporaryFile>();
        bool result = decompress::raw(m_temp_file->path(), json_temp_file->path());
        if (!result)
        {
            LOG_WARNING << "Could not decompress " << m_temp_file->path();
        }
        std::swap(json_temp_file, m_temp_file);
        return result;
    }

    void MSubdirData::create_target(nlohmann::json& mod_etag)
    {
        m_temp_file = std::make_unique<TemporaryFile>();
        m_progress_bar = Console::instance().add_progress_bar(m_name);
        m_target = std::make_unique<DownloadTarget>(m_name, m_url, m_temp_file->path());
        m_target->set_progress_bar(m_progress_bar);
        m_target->set_finalize_callback(&MSubdirData::finalize_transfer, this);
        m_target->set_mod_etag_headers(mod_etag);
    }

    std::size_t MSubdirData::get_cache_control_max_age(const std::string& val)
    {
        std::regex max_age_re("max-age=(\\d+)");
        std::smatch max_age_match;
        bool matches = std::regex_search(val, max_age_match, max_age_re);
        if (!matches) return 0;
        return std::stoi(max_age_match[1]);
    }

    nlohmann::json MSubdirData::read_mod_and_etag()
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
            while (s.get(next))
            {
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
        try
        {
            result = nlohmann::json::parse(json);
            return result;
        }
        catch(...)
        {
            LOG_WARNING << "Could not parse mod / etag header!";
            return nlohmann::json();
        }
    }
}

