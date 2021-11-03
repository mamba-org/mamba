// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"

namespace decompress
{
    bool raw(const std::string& in, const std::string& out)
    {
        int r;
        std::ptrdiff_t size;

        LOG_INFO << "Decompressing from " << in << " to " << out;

        struct archive* a = archive_read_new();
        archive_read_support_filter_bzip2(a);
        archive_read_support_format_raw(a);
        // TODO figure out good value for this
        const std::size_t BLOCKSIZE = 16384;
        r = archive_read_open_filename(a, in.c_str(), BLOCKSIZE);
        if (r != ARCHIVE_OK)
        {
            return false;
        }

        struct archive_entry* entry;
        std::ofstream out_file(out);
        char buff[BLOCKSIZE];
        std::size_t buffsize = BLOCKSIZE;
        r = archive_read_next_header(a, &entry);
        if (r != ARCHIVE_OK)
        {
            return false;
        }

        while (true)
        {
            size = archive_read_data(a, &buff, buffsize);
            if (size < ARCHIVE_OK)
            {
                throw std::runtime_error(std::string("Could not read archive: ")
                                         + archive_error_string(a));
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
}  // namespace decompress

namespace mamba
{
    MSubdirData::MSubdirData(const std::string& name,
                             const std::string& repodata_url,
                             const std::string& repodata_fn,
                             MultiPackageCache& caches,
                             bool is_noarch)
        : m_loaded(false)
        , m_download_complete(false)
        , m_repodata_url(repodata_url)
        , m_name(name)
        , m_json_fn(repodata_fn)
        , m_solv_fn(repodata_fn.substr(0, repodata_fn.size() - 4) + "solv")
        , m_caches(caches)
        , m_is_noarch(is_noarch)
    {
    }

    fs::file_time_type::duration MSubdirData::check_cache(
        const fs::path& cache_file, const fs::file_time_type::clock::time_point& ref)
    {
        try
        {
            auto last_write = fs::last_write_time(cache_file);
            auto tdiff = ref - last_write;
            return tdiff;
        }
        catch (...)
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
        return starts_with(m_repodata_url, "file://");
    }

    bool MSubdirData::load()
    {
        auto now = fs::file_time_type::clock::now();

        m_valid_cache_path = "";
        m_expired_cache_path = "";
        m_loaded = false;
        m_mod_etag = nlohmann::json();

        LOG_INFO << "Searching index cache file for repo '" << m_repodata_url << "'";

        for (const auto& cache_path : m_caches.paths())
        {
            auto json_file = cache_path / "cache" / m_json_fn;
            auto solv_file = cache_path / "cache" / m_solv_fn;

            if (!fs::exists(json_file))
                continue;

            auto lock = LockFile::try_lock(cache_path / "cache");
            auto cache_age = check_cache(json_file, now);

            if (cache_age != fs::file_time_type::duration::max() && !forbid_cache())
            {
                LOG_INFO << "Found cache at '" << json_file.string() << "'";
                m_mod_etag = read_mod_and_etag(json_file);

                if (m_mod_etag.size() != 0)
                {
                    int max_age = 0;
                    if (Context::instance().local_repodata_ttl > 1)
                    {
                        max_age = Context::instance().local_repodata_ttl;
                    }
                    else if (Context::instance().local_repodata_ttl == 1)
                    {
                        // TODO error handling if _cache_control key does not exist!
                        auto el = m_mod_etag.value("_cache_control", std::string(""));
                        max_age = get_cache_control_max_age(el);
                    }

                    auto cache_age_seconds
                        = std::chrono::duration_cast<std::chrono::seconds>(cache_age).count();
                    if ((max_age > cache_age_seconds || Context::instance().offline))
                    {
                        // valid json cache found
                        if (!m_loaded)
                        {
                            LOG_DEBUG << "Using JSON cache";
                            LOG_TRACE << "Cache age: " << cache_age_seconds << "/" << max_age
                                      << "s";

                            m_valid_cache_path = cache_path;
                            m_json_cache_valid = true;
                            m_loaded = true;
                        }

                        // check libsolv cache
                        auto solv_age = check_cache(solv_file, now);
                        if (solv_age != fs::file_time_type::duration::max()
                            && solv_age.count() <= cache_age.count())
                        {
                            // valid libsolv cache found
                            LOG_DEBUG << "Using SOLV cache";
                            LOG_TRACE << "Cache age: "
                                      << std::chrono::duration_cast<std::chrono::seconds>(solv_age)
                                             .count()
                                      << "s";
                            m_solv_cache_valid = true;
                            m_valid_cache_path = cache_path;
                            // no need to search for other valid caches
                            break;
                        }
                    }
                    else
                    {
                        m_expired_cache_path = cache_path;
                        LOG_DEBUG << "Expired cache or invalid mod/etag headers";
                    }
                }
                else
                {
                    LOG_DEBUG << "Could not determine cache mod/etag headers";
                }
            }
        }

        if (m_loaded)
        {
            std::string prefix = m_name;
            prefix.resize(PREFIX_LENGTH - 1, ' ');
            Console::stream() << prefix << " Using cache";
        }
        else
        {
            LOG_INFO << "No valid cache found";
            if (!Context::instance().offline || forbid_cache())
                create_target(m_mod_etag);
        }
        return true;
    }

    std::string MSubdirData::cache_path() const
    {
        // TODO invalidate solv cache on version updates!!
        if (m_json_cache_valid && m_solv_cache_valid)
        {
            return m_valid_cache_path / "cache" / m_solv_fn;
        }
        else if (m_json_cache_valid)
        {
            return m_valid_cache_path / "cache" / m_json_fn;
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
        if (m_target->result != 0 || m_target->http_status >= 400)
        {
            LOG_INFO << "Unable to retrieve repodata (response: " << m_target->http_status
                     << ") for '" << m_repodata_url << "'";
            m_progress_bar.set_postfix(std::to_string(m_target->http_status) + " Failed");
            m_progress_bar.set_full();
            m_progress_bar.mark_as_completed();
            m_loaded = false;
            return false;
        }

        LOG_DEBUG << "HTTP response code: " << m_target->http_status;
        // Note HTTP status == 0 for files
        if (m_target->http_status == 0 || m_target->http_status == 200
            || m_target->http_status == 304)
        {
            m_download_complete = true;
        }
        else
        {
            LOG_WARNING << "HTTP response code indicates error, retrying.";
            throw std::runtime_error("Unhandled HTTP code: "
                                     + std::to_string(m_target->http_status));
        }

        fs::path json_file, solv_file;
        fs::path writable_cache_path = m_caches.first_writable_path();

        if (m_target->http_status == 304)
        {
            // cache still valid
            LOG_INFO << "Cache is still valid";
            json_file = m_expired_cache_path / "cache" / m_json_fn;
            solv_file = m_expired_cache_path / "cache" / m_solv_fn;

            auto now = fs::file_time_type::clock::now();
            auto json_age = check_cache(json_file, now);
            auto solv_age = check_cache(solv_file, now);

            if (path::is_writable(json_file)
                && (!fs::exists(solv_file) || path::is_writable(solv_file)))
            {
                LOG_DEBUG << "Refreshing cache files ages";
                m_valid_cache_path = m_expired_cache_path;
            }
            else
            {
                if (writable_cache_path.empty())
                {
                    LOG_ERROR << "Could not find any writable cache directory for repodata file";
                    throw std::runtime_error("Non-writable cache error.");
                }

                LOG_DEBUG << "Copying repodata cache files from '" << m_expired_cache_path.string()
                          << "' to '" << writable_cache_path.string() << "'";
                fs::path writable_cache_dir = create_cache_dir(writable_cache_path);
                auto lock = LockFile(writable_cache_dir);

                auto copied_json_file = writable_cache_dir / m_json_fn;
                fs::copy(json_file, copied_json_file);
                json_file = copied_json_file;

                if (fs::exists(solv_file))
                {
                    auto copied_solv_file = writable_cache_dir / m_solv_fn;
                    fs::copy(solv_file, copied_solv_file);
                    solv_file = copied_solv_file;
                }

                m_valid_cache_path = writable_cache_path;
            }


            {
                LOG_TRACE << "Refreshing '" << json_file.string() << "'";
                auto lock = LockFile(json_file);
                fs::last_write_time(json_file, now);
            }
            if (fs::exists(solv_file) && solv_age.count() <= json_age.count())
            {
                LOG_TRACE << "Refreshing '" << solv_file.string() << "'";
                auto lock = LockFile(solv_file);
                fs::last_write_time(solv_file, now);
                m_solv_cache_valid = true;
            }

            m_progress_bar.set_postfix("No change");
            m_progress_bar.set_full();
            m_progress_bar.mark_as_completed();

            m_json_cache_valid = true;
            m_loaded = true;
            m_temp_file.reset(nullptr);
            return true;
        }
        else
        {
            if (writable_cache_path.empty())
            {
                LOG_ERROR << "Could not find any writable cache directory for repodata file";
                throw std::runtime_error("Non-writable cache error.");
            }
        }

        LOG_DEBUG << "Finalized transfer of '" << m_repodata_url << "'";

        fs::path writable_cache_dir = create_cache_dir(writable_cache_path);
        json_file = writable_cache_dir / m_json_fn;
        auto lock = LockFile(writable_cache_dir);

        m_mod_etag.clear();
        m_mod_etag["_url"] = m_repodata_url;
        m_mod_etag["_etag"] = m_target->etag;
        m_mod_etag["_mod"] = m_target->mod;
        m_mod_etag["_cache_control"] = m_target->cache_control;

        LOG_DEBUG << "Opening '" << json_file.string() << "'";
        path::touch(json_file, true);
        std::ofstream final_file(json_file);

        if (!final_file.is_open())
        {
            LOG_ERROR << "Could not open file '" << json_file.string() << "'";
            exit(1);
        }

        if (ends_with(m_repodata_url, ".bz2"))
        {
            m_progress_bar.set_postfix("Decomp...");
            decompress();
        }

        m_progress_bar.set_postfix("Finalizing...");

        std::ifstream temp_file(m_temp_file->path());
        std::stringstream temp_json;
        temp_json << m_mod_etag.dump();

        // replace `}` with `,`
        temp_json.seekp(-1, temp_json.cur);
        temp_json << ',';
        final_file << temp_json.str();
        temp_file.seekg(1);
        std::copy(std::istreambuf_iterator<char>(temp_file),
                  std::istreambuf_iterator<char>(),
                  std::ostreambuf_iterator<char>(final_file));

        if (!temp_file)
        {
            LOG_ERROR << "Could not write out repodata file '" << json_file
                      << "': " << strerror(errno);
            fs::remove(json_file);
            exit(1);
        }

        m_progress_bar.set_postfix("Done");
        m_progress_bar.set_full();
        m_progress_bar.mark_as_completed();

        m_valid_cache_path = writable_cache_path;
        m_json_cache_valid = true;
        m_loaded = true;

        temp_file.close();
        m_temp_file.reset(nullptr);
        final_file.close();

        fs::last_write_time(json_file, fs::file_time_type::clock::now());

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
        m_target = std::make_unique<DownloadTarget>(m_name, m_repodata_url, m_temp_file->path());
        m_target->set_progress_bar(m_progress_bar);
        // if we get something _other_ than the noarch, we DO NOT throw if the file
        // can't be retrieved
        if (!m_is_noarch)
        {
            m_target->set_ignore_failure(true);
        }
        m_target->set_finalize_callback(&MSubdirData::finalize_transfer, this);
        m_target->set_mod_etag_headers(mod_etag);
    }

    std::size_t MSubdirData::get_cache_control_max_age(const std::string& val)
    {
        static std::regex max_age_re("max-age=(\\d+)");
        std::smatch max_age_match;
        bool matches = std::regex_search(val, max_age_match, max_age_re);
        if (!matches)
            return 0;
        return std::stoi(max_age_match[1]);
    }

    nlohmann::json MSubdirData::read_mod_and_etag(const fs::path& file)
    {
        // parse json at the beginning of the stream such as
        // {"_url": "https://conda.anaconda.org/conda-forge/linux-64",
        // "_etag": "W/\"6092e6a2b6cec6ea5aade4e177c3edda-8\"",
        // "_mod": "Sat, 04 Apr 2020 03:29:49 GMT",
        // "_cache_control": "public, max-age=1200"

        auto extract_subjson = [](std::ifstream& s) {
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

        std::ifstream in_file(file);
        auto json = extract_subjson(in_file);
        nlohmann::json result;
        try
        {
            result = nlohmann::json::parse(json);
            return result;
        }
        catch (...)
        {
            LOG_WARNING << "Could not parse mod/etag header";
            return nlohmann::json();
        }
    }

    std::string cache_fn_url(const std::string& url)
    {
        return cache_name_from_url(url) + ".json";
    }

    std::string create_cache_dir(const fs::path& cache_path)
    {
        std::string cache_dir = cache_path / "cache";
        fs::create_directories(cache_dir);
#ifndef _WIN32
        ::chmod(cache_dir.c_str(), 02775);
#endif
        return cache_dir;
    }

    MRepo MSubdirData::create_repo(MPool& pool)
    {
        RepoMetadata meta{ m_repodata_url,
                           Context::instance().add_pip_as_python_dependency,
                           m_mod_etag["_etag"],
                           m_mod_etag["_mod"] };

        return MRepo(pool, m_name, cache_path(), meta);
    }

    void MSubdirData::clear_cache()
    {
        if (fs::exists(m_json_fn))
        {
            fs::remove(m_json_fn);
        }
        if (fs::exists(m_solv_fn))
        {
            fs::remove(m_solv_fn);
        }
    }
}  // namespace mamba
