// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <stdexcept>

#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

#include "progress_bar_impl.hpp"

namespace mamba
{
    /*******************
     * MSubdirMetadata *
     *******************/
    
    namespace
    {
        // parse json at the beginning of the stream such as
        // {"_url": "https://conda.anaconda.org/conda-forge/linux-64",
        // "_etag": "W/\"6092e6a2b6cec6ea5aade4e177c3edda-8\"",
        // "_mod": "Sat, 04 Apr 2020 03:29:49 GMT",
        // "_cache_control": "public, max-age=1200"
        std::string extract_subjson(std::ifstream& s)
        {
            char next = {};
            std::string result = {};
            bool escaped = false;
            int i = 0, N = 4;
            std::size_t idx = 0;
            std::size_t prev_keystart = 0;
            bool in_key = false;
            std::string key = "";

            while (s.get(next))
            {
                idx++;
                if (next == '"')
                {
                    if (!escaped)
                    {
                        if ((i / 2) % 2 == 0)
                        {
                            in_key = !in_key;
                            if (in_key)
                            {
                                prev_keystart = idx + 1;
                            }
                            else
                            {
                                if (key != "_mod" && key != "_etag" && key != "_cache_control"
                                    && key != "_url")
                                {
                                    // bail out
                                    auto last_comma = result.find_last_of(",", prev_keystart - 2);
                                    if (last_comma != std::string::npos && last_comma > 0)
                                    {
                                        result = result.substr(0, last_comma);
                                        result.push_back('}');
                                        return result;
                                    }
                                    else
                                    {
                                        return std::string();
                                    }
                                }
                                key.clear();
                            }
                        }
                        i++;
                    }

                    // 4 keys == 4 ticks
                    if (i == 4 * N)
                    {
                        result += "\"}";
                        return result;
                    }
                }

                if (in_key && next != '"')
                {
                    key.push_back(next);
                }

                escaped = (!escaped && next == '\\');
                result.push_back(next);
            }
            return std::string();
        }
    }

    auto MSubdirMetadata::read(const fs::u8path& file) -> expected_subdir_metadata
    {
        fs::u8path state_file = file;
        state_file.replace_extension(".state.json");
        std::error_code ec;
        if (fs::exists(state_file, ec))
        {
            return from_state_file(state_file, file);
        }
        else
        {
            return from_repodata_file(file);
        }
    }
    
    void MSubdirMetadata::write(const fs::u8path& file)
    {
        nlohmann::json j;
        j["url"] = m_url;
        j["etag"] = m_etag;
        j["mod"] = m_mod;
        j["cache_control"] = m_cache_control;
        j["size"] = m_stored_file_size;

        auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            m_stored_mtime.time_since_epoch()
        );
        j["mtime_ns"] = nsecs.count();

        if (m_has_zst.has_value())
        {
            j["has_zst"]["value"] = m_has_zst.value().value;
            j["has_zst"]["last_checked"] = timestamp(m_has_zst.value().last_checked);
        }
        std::ofstream out = open_ofstream(file);
        out << j.dump(4);
    }

    bool MSubdirMetadata::check_valid_metadata(const fs::u8path& file)
    {
        if (m_stored_file_size != fs::file_size(file))
        {
            LOG_INFO << "File size changed, invalidating metadata";
            return false;
        }
#ifndef _WIN32
        bool last_write_time_valid = fs::last_write_time(file) == m_stored_mtime;
#else
        bool last_write_time_valid = filetime_to_unix(fs::last_write_time(file)) == m_stored_mtime;
#endif
        if (!last_write_time_valid)
        {
            LOG_INFO << "File mtime changed, invalidating metadata";
        }
        return last_write_time_valid;
    }

    const std::string& MSubdirMetadata::url() const
    {
        return m_url;
    }

    const std::string& MSubdirMetadata::etag() const
    {
        return m_etag;
    }

    const std::string& MSubdirMetadata::mod() const
    {
        return m_mod;
    }

    const std::string& MSubdirMetadata::cache_control() const
    {
        return m_cache_control;
    }

    bool MSubdirMetadata::has_zst() const
    {
        return m_has_zst.has_value() && m_has_zst.value().value && !m_has_zst.value().has_expired();
    }

    void MSubdirMetadata::store_http_metadata(std::string url, std::string etag, std::string mod, std::string cache_control)
    {
        m_url = std::move(url);
        m_etag = std::move(etag);
        m_mod = std::move(mod);
        m_cache_control = std::move(cache_control);
    }

    void MSubdirMetadata::store_file_metadata(const fs::u8path& file)
    {
#ifndef _WIN32
        m_stored_mtime = fs::last_write_time(file);
#else
        // convert windows filetime to unix timestamp
        m_stored_mtime = filetime_to_unix(fs::last_write_time(file));
#endif
        m_stored_file_size = fs::file_size(file);
    }

    void MSubdirMetadata::set_zst(bool value)
    {
        m_has_zst = { value,
                      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    }

    auto MSubdirMetadata::from_state_file(const fs::u8path& state_file, const fs::u8path& repodata_file) -> expected_subdir_metadata 
    {
        std::ifstream infile = open_ifstream(state_file);
        nlohmann::json j = nlohmann::json::parse(infile);
        MSubdirMetadata m;
        try
        {
            m.m_url = j["url"].get<std::string>();
            m.m_etag = j["etag"].get<std::string>();
            m.m_mod = j["mod"].get<std::string>();
            m.m_cache_control = j["cache_control"].get<std::string>();
            m.m_stored_file_size = j["size"].get<std::size_t>();

            using time_type = decltype(m.m_stored_mtime);
            m.m_stored_mtime = time_type(std::chrono::duration_cast<time_type::duration>(
                std::chrono::nanoseconds(j["mtime_ns"].get<std::size_t>())
            ));

            int err_code = 0;
            if (j.find("has_zst") != j.end())
            {
                m.m_has_zst = {
                    j["has_zst"]["value"].get<bool>(),
                    parse_utc_timestamp(j["has_zst"]["last_checked"].get<std::string>(), err_code)
                };
            }
        }
        catch (const std::exception& e)
        {
            LOG_WARNING << "Could not parse state file: " << e.what();
            std::error_code ec;
            fs::remove(state_file, ec);
            if (ec)
            {
                LOG_WARNING << "Could not remove state file " << state_file << ": "
                            << ec.message();
            }
            return make_unexpected(
                fmt::format("File: {}: Could not load cache state: {}", state_file, e.what()),
                mamba_error_code::cache_not_loaded
            );
        }

        if (!m.check_valid_metadata(repodata_file))
        {
            LOG_WARNING << "Cache file " << repodata_file << " was modified by another program";
            return make_unexpected(
                fmt::format("File: {}: Cache file mtime mismatch", state_file),
                mamba_error_code::cache_not_loaded
            );
        }
        return m;
    }
    
    auto MSubdirMetadata::from_repodata_file(const fs::u8path& repodata_file) -> expected_subdir_metadata
    {
        std::ifstream in_file = open_ifstream(repodata_file);
        std::string json = extract_subjson(in_file);

        try
        {
            nlohmann::json result = nlohmann::json::parse(json);
            MSubdirMetadata m;
            m.m_url = result.value("_url", "");
            m.m_etag = result.value("_etag", "");
            m.m_mod = result.value("_mod", "");
            m.m_cache_control = result.value("_cache_control", "");
            return m;
        }
        catch (std::exception& e)
        {
            LOG_WARNING << "Could not parse mod/etag header";
            return make_unexpected(
                fmt::format("File: {}: Could not parse mod/etag header ({})", repodata_file, e.what()),
                mamba_error_code::cache_not_loaded
            );
        }
    }


    bool MSubdirMetadata::checked_at::has_expired() const
    {
        // difference in seconds, check every 14 days
        return std::difftime(std::time(nullptr), last_checked) > 60 * 60 * 24 * 14;
    }

    /**********************
     * OLD IMPLEMENTATION *
     **********************/

    expected_t<MSubdirData> MSubdirData::create(
        ChannelContext& channel_context,
        const Channel& channel,
        const std::string& platform,
        const std::string& url,
        MultiPackageCache& caches,
        const std::string& repodata_fn
    )
    {
        try
        {
            return MSubdirData(channel_context, channel, platform, url, caches, repodata_fn);
        }
        catch (std::exception& e)
        {
            return make_unexpected(e.what(), mamba_error_code::subdirdata_not_loaded);
        }
        catch (...)
        {
            return make_unexpected(
                "Unkown error when trying to load subdir data " + url,
                mamba_error_code::unknown
            );
        }
    }

    MSubdirData::MSubdirData(
        ChannelContext& channel_context,
        const Channel& channel,
        const std::string& platform,
        const std::string& url,
        MultiPackageCache& caches,
        const std::string& repodata_fn
    )
        : m_target(nullptr)
        , m_writable_pkgs_dir(caches.first_writable_path())
        , m_progress_bar()
        , m_loaded(false)
        , m_download_complete(false)
        , m_repodata_url(util::concat(url, "/", repodata_fn))
        , m_name(util::join_url(channel.canonical_name(), platform))
        , m_is_noarch(platform == "noarch")
        , p_channel(&channel)
        , p_context(&channel_context.context())
    {
        assert(p_context);
        m_json_fn = cache_fn_url(m_repodata_url);
        m_solv_fn = m_json_fn.substr(0, m_json_fn.size() - 4) + "solv";
        load(caches, channel_context);
    }

    MSubdirData::MSubdirData(MSubdirData&& rhs)
        : m_target(std::move(rhs.m_target))
        , m_check_targets(std::move(rhs.m_check_targets))
        , m_json_cache_valid(rhs.m_json_cache_valid)
        , m_solv_cache_valid(rhs.m_solv_cache_valid)
        , m_valid_cache_path(std::move(rhs.m_valid_cache_path))
        , m_expired_cache_path(std::move(rhs.m_expired_cache_path))
        , m_writable_pkgs_dir(std::move(rhs.m_writable_pkgs_dir))
        , m_progress_bar(std::move(rhs.m_progress_bar))
        , m_progress_bar_check(std::move(rhs.m_progress_bar_check))
        , m_loaded(rhs.m_loaded)
        , m_download_complete(rhs.m_download_complete)
        , m_repodata_url(std::move(rhs.m_repodata_url))
        , m_name(std::move(rhs.m_name))
        , m_json_fn(std::move(rhs.m_json_fn))
        , m_solv_fn(std::move(rhs.m_solv_fn))
        , m_is_noarch(rhs.m_is_noarch)
        , m_metadata(std::move(rhs.m_metadata))
        , m_temp_file(std::move(rhs.m_temp_file))
        , p_channel(rhs.p_channel)
        , p_context(rhs.p_context)
    {
        if (m_target != nullptr)
        {
            m_target->set_finalize_callback(&MSubdirData::finalize_transfer, this);
        }
        for (auto& t : m_check_targets)
        {
            t->set_finalize_callback(&MSubdirData::finalize_check, this);
        }
    }

    MSubdirData& MSubdirData::operator=(MSubdirData&& rhs)
    {
        using std::swap;
        swap(m_target, rhs.m_target);
        swap(m_json_cache_valid, rhs.m_json_cache_valid);
        swap(m_solv_cache_valid, rhs.m_solv_cache_valid);
        swap(m_valid_cache_path, rhs.m_valid_cache_path);
        swap(m_expired_cache_path, rhs.m_expired_cache_path);
        swap(m_writable_pkgs_dir, rhs.m_writable_pkgs_dir);
        swap(m_progress_bar, m_progress_bar);
        swap(m_progress_bar_check, m_progress_bar_check);
        swap(m_loaded, rhs.m_loaded);
        swap(m_download_complete, rhs.m_download_complete);
        swap(m_repodata_url, rhs.m_repodata_url);
        swap(m_name, rhs.m_name);
        swap(m_json_fn, rhs.m_json_fn);
        swap(m_solv_fn, rhs.m_solv_fn);
        swap(m_is_noarch, rhs.m_is_noarch);
        swap(m_metadata, rhs.m_metadata);
        swap(m_temp_file, rhs.m_temp_file);
        swap(m_check_targets, rhs.m_check_targets);
        swap(p_channel, rhs.p_channel);
        swap(p_context, rhs.p_context);

        if (m_target != nullptr)
        {
            m_target->set_finalize_callback(&MSubdirData::finalize_transfer, this);
        }
        if (rhs.m_target != nullptr)
        {
            rhs.m_target->set_finalize_callback(&MSubdirData::finalize_transfer, &rhs);
        }

        for (auto& t : m_check_targets)
        {
            t->set_finalize_callback(&MSubdirData::finalize_check, this);
        }
        for (auto& t : rhs.m_check_targets)
        {
            t->set_finalize_callback(&MSubdirData::finalize_check, &rhs);
        }

        return *this;
    }

    fs::file_time_type::duration MSubdirData::check_cache(
        const fs::u8path& cache_file,
        const fs::file_time_type::clock::time_point& ref
    ) const
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

    bool MSubdirData::loaded() const
    {
        return m_loaded;
    }

    bool MSubdirData::forbid_cache()
    {
        return util::starts_with(m_repodata_url, "file://");
    }

    void MSubdirData::finalize_checks()
    {
        create_target();
    }

    bool MSubdirData::finalize_check(const DownloadTarget& target)
    {
        LOG_INFO << "Checked: " << target.get_url() << " [" << target.get_http_status() << "]";
        if (m_progress_bar_check)
        {
            m_progress_bar_check.repr().postfix.set_value("Checked");
            m_progress_bar_check.repr().speed.deactivate();
            m_progress_bar_check.repr().total.deactivate();
            m_progress_bar_check.mark_as_completed();
        }

        if (util::ends_with(target.get_url(), ".zst"))
        {
            this->m_metadata.set_zst(target.get_http_status() == 200);
        }
        return true;
    }

    std::vector<std::unique_ptr<DownloadTarget>>& MSubdirData::check_targets()
    {
        // check if zst or (later) jlap are available
        return m_check_targets;
    }

    namespace
    {

        template <typename T>
        std::vector<T> without_duplicates(std::vector<T>&& values)
        {
            const auto end_it = std::unique(values.begin(), values.end());
            values.erase(end_it, values.end());
            return values;
        }

        bool check_zst(ChannelContext& channel_context, const Channel& channel)
        {
            for (const auto& c : channel_context.context().repodata_has_zst)
            {
                if (channel_context.make_channel(c) == channel)
                {
                    return true;
                }
            }
            return false;
        }
    }

    bool MSubdirData::load(MultiPackageCache& caches, ChannelContext& channel_context)
    {
        assert(&channel_context.context() == p_context);
        assert(p_context != nullptr);

        auto now = fs::file_time_type::clock::now();

        m_valid_cache_path = "";
        m_expired_cache_path = "";
        m_loaded = false;

        LOG_INFO << "Searching index cache file for repo '" << m_repodata_url << "'";

        const auto cache_paths = without_duplicates(caches.paths());

        auto& context = *p_context;

        for (const auto& cache_path : cache_paths)
        {
            auto json_file = cache_path / "cache" / m_json_fn;
            auto solv_file = cache_path / "cache" / m_solv_fn;

            std::error_code ec;
            if (!fs::exists(json_file, ec))
            {
                continue;
            }

            auto lock = LockFile(cache_path / "cache");
            auto cache_age = check_cache(json_file, now);

            if (cache_age != fs::file_time_type::duration::max() && !forbid_cache())
            {
                auto metadata_temp = MSubdirMetadata::read(json_file);
                if (!metadata_temp.has_value())
                {
                    LOG_INFO << "Invalid json cache found, ignoring";
                    continue;
                }
                if (metadata_temp.has_value())
                {
                    m_metadata = std::move(metadata_temp.value());
                    int max_age = 0;
                    if (context.local_repodata_ttl > 1)
                    {
                        max_age = static_cast<int>(context.local_repodata_ttl);
                    }
                    else if (context.local_repodata_ttl == 1)
                    {
                        // TODO error handling if _cache_control key does not exist!
                        max_age = static_cast<int>(get_cache_control_max_age(m_metadata.cache_control())
                        );
                    }

                    auto cache_age_seconds = std::chrono::duration_cast<std::chrono::seconds>(cache_age)
                                                 .count();
                    if ((max_age > cache_age_seconds || context.offline || context.use_index_cache))
                    {
                        // valid json cache found
                        if (!m_loaded)
                        {
                            LOG_DEBUG << "Using JSON cache";
                            LOG_TRACE << "Cache age: " << cache_age_seconds << "/" << max_age << "s";

                            m_valid_cache_path = cache_path;
                            m_json_cache_valid = true;
                            m_loaded = true;
                        }

                        // check libsolv cache
                        auto solv_age = check_cache(solv_file, now);
                        if (solv_age != fs::file_time_type::duration::max() && solv_age <= cache_age)
                        {
                            // valid libsolv cache found
                            LOG_DEBUG << "Using SOLV cache";
                            LOG_TRACE
                                << "Cache age: "
                                << std::chrono::duration_cast<std::chrono::seconds>(solv_age).count()
                                << "s";
                            m_solv_cache_valid = true;
                            m_valid_cache_path = cache_path;
                            // no need to search for other valid caches
                            break;
                        }
                    }
                    else
                    {
                        if (m_expired_cache_path.empty())
                        {
                            m_expired_cache_path = cache_path;
                        }
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
            Console::stream() << fmt::format("{:<50} {:>20}", m_name, std::string("Using cache"));
        }
        else
        {
            LOG_INFO << "No valid cache found";
            if (!m_expired_cache_path.empty())
            {
                LOG_INFO << "Expired cache (or invalid mod/etag headers) found at '"
                         << m_expired_cache_path.string() << "'";
            }

            if (!context.offline || forbid_cache())
            {
                if (context.repodata_use_zst)
                {
                    bool has_zst = m_metadata.has_zst();
                    if (!has_zst)
                    {
                        has_zst = check_zst(channel_context, *p_channel);
                        m_metadata.set_zst(has_zst);
                    }
                    if (!has_zst)
                    {
                        m_check_targets.push_back(std::make_unique<DownloadTarget>(
                            context,
                            m_name + " (check zst)",
                            m_repodata_url + ".zst",
                            ""
                        ));
                        m_check_targets.back()->set_head_only(true);
                        m_check_targets.back()->set_finalize_callback(&MSubdirData::finalize_check, this);
                        m_check_targets.back()->set_ignore_failure(true);
                        if (!(context.graphics_params.no_progress_bars
                              || context.output_params.quiet || context.output_params.json))
                        {
                            m_progress_bar_check = Console::instance().add_progress_bar(
                                m_name + " (check zst)"
                            );
                            m_check_targets.back()->set_progress_bar(m_progress_bar_check);
                            m_progress_bar_check.repr().postfix.set_value("Checking");
                        }
                        return true;
                    }
                }
                create_target();
            }
        }
        return true;
    }

    expected_t<std::string> MSubdirData::cache_path() const
    {
        // TODO invalidate solv cache on version updates!!
        if (m_json_cache_valid && m_solv_cache_valid)
        {
            return (m_valid_cache_path / "cache" / m_solv_fn).string();
        }
        else if (m_json_cache_valid)
        {
            return (m_valid_cache_path / "cache" / m_json_fn).string();
        }
        return make_unexpected("Cache not loaded", mamba_error_code::cache_not_loaded);
    }

    DownloadTarget* MSubdirData::target()
    {
        return m_target.get();
    }

    const std::string& MSubdirData::name() const
    {
        return m_name;
    }

    void
    MSubdirData::refresh_last_write_time(const fs::u8path& json_file, const fs::u8path& solv_file)
    {
        auto now = fs::file_time_type::clock::now();

        auto json_age = check_cache(json_file, now);
        auto solv_age = check_cache(solv_file, now);

        {
            auto lock = LockFile(json_file);
            fs::last_write_time(json_file, fs::now());
        }

        if (fs::exists(solv_file) && solv_age.count() <= json_age.count())
        {
            auto lock = LockFile(solv_file);
            fs::last_write_time(solv_file, fs::now());
            m_solv_cache_valid = true;
        }

        auto state_file = json_file;
        state_file.replace_extension(".state.json");
        auto lock = LockFile(state_file);
        m_metadata.store_file_metadata(json_file);
        m_metadata.write(state_file);
    }

    bool MSubdirData::finalize_transfer(const DownloadTarget&)
    {
        if (m_target->get_result() != 0 || m_target->get_http_status() >= 400)
        {
            LOG_INFO << "Unable to retrieve repodata (response: " << m_target->get_http_status()
                     << ") for '" << m_target->get_url() << "'";

            if (m_progress_bar)
            {
                m_progress_bar.set_postfix(std::to_string(m_target->get_http_status()) + " failed");
                m_progress_bar.set_full();
                m_progress_bar.mark_as_completed();
            }
            m_loaded = false;
            return false;
        }

        LOG_DEBUG << "HTTP response code: " << m_target->get_http_status();
        // Note HTTP status == 0 for files
        if (m_target->get_http_status() == 0 || m_target->get_http_status() == 200
            || m_target->get_http_status() == 304)
        {
            m_download_complete = true;
        }
        else
        {
            LOG_WARNING << "HTTP response code indicates error, retrying.";
            throw mamba_error(
                "Unhandled HTTP code: " + std::to_string(m_target->get_http_status()),
                mamba_error_code::subdirdata_not_loaded
            );
        }

        fs::u8path json_file, solv_file;

        if (m_target->get_http_status() == 304)
        {
            // cache still valid
            LOG_INFO << "Cache is still valid";
            json_file = m_expired_cache_path / "cache" / m_json_fn;
            solv_file = m_expired_cache_path / "cache" / m_solv_fn;

            if (path::is_writable(json_file)
                && (!fs::exists(solv_file) || path::is_writable(solv_file)))
            {
                LOG_DEBUG << "Refreshing cache files ages";
                m_valid_cache_path = m_expired_cache_path;
            }
            else
            {
                if (m_writable_pkgs_dir.empty())
                {
                    LOG_ERROR << "Could not find any writable cache directory for repodata file";
                    throw mamba_error(
                        "Non-writable cache error.",
                        mamba_error_code::subdirdata_not_loaded
                    );
                }

                LOG_DEBUG << "Copying repodata cache files from '" << m_expired_cache_path.string()
                          << "' to '" << m_writable_pkgs_dir.string() << "'";
                fs::u8path writable_cache_dir = create_cache_dir(m_writable_pkgs_dir);
                auto lock = LockFile(writable_cache_dir);

                auto copied_json_file = writable_cache_dir / m_json_fn;
                if (fs::exists(copied_json_file))
                {
                    fs::remove(copied_json_file);
                }
                fs::copy(json_file, copied_json_file);
                json_file = copied_json_file;

                if (fs::exists(solv_file))
                {
                    auto copied_solv_file = writable_cache_dir / m_solv_fn;
                    if (fs::exists(copied_solv_file))
                    {
                        fs::remove(copied_solv_file);
                    }
                    fs::copy(solv_file, copied_solv_file);
                    solv_file = copied_solv_file;
                }

                m_valid_cache_path = m_writable_pkgs_dir;
            }

            refresh_last_write_time(json_file, solv_file);

            if (m_progress_bar)
            {
                auto& r = m_progress_bar.repr();
                r.postfix.set_format("{:>20}", 20);
                r.prefix.set_format("{:<50}", 50);

                m_progress_bar.set_postfix("No change");
                m_progress_bar.mark_as_completed();

                r.total.deactivate();
                r.speed.deactivate();
                r.elapsed.deactivate();
            }

            m_json_cache_valid = true;
            m_loaded = true;
            m_temp_file.reset();
            return true;
        }
        else
        {
            if (m_writable_pkgs_dir.empty())
            {
                LOG_ERROR << "Could not find any writable cache directory for repodata file";
                throw mamba_error("Non-writable cache error.", mamba_error_code::subdirdata_not_loaded);
            }
        }

        LOG_DEBUG << "Finalized transfer of '" << m_target->get_url() << "'";

        fs::u8path writable_cache_dir = create_cache_dir(m_writable_pkgs_dir);
        json_file = writable_cache_dir / m_json_fn;
        auto lock = LockFile(writable_cache_dir);

        //auto file_size = fs::file_size(m_temp_file->path());

        m_metadata.store_http_metadata(
            m_target->get_url(),
            m_target->get_etag(),
            m_target->get_mod(),
            m_target->get_cache_control()
        );
        //m_metadata.stored_file_size = file_size;

        fs::u8path state_file = json_file;
        state_file.replace_extension(".state.json");
        std::error_code ec;
        mamba_fs::rename_or_move(m_temp_file->path(), json_file, ec);
        if (ec)
        {
            throw mamba_error(
                fmt::format(
                    "Could not move repodata file from {} to {}: {}",
                    m_temp_file->path(),
                    json_file,
                    strerror(errno)
                ),
                mamba_error_code::subdirdata_not_loaded
            );
        }
        fs::last_write_time(json_file, fs::now());

        m_metadata.store_file_metadata(json_file);
        m_metadata.write(state_file);

        if (m_progress_bar)
        {
            m_progress_bar.repr().postfix.set_value("Downloaded").deactivate();
            m_progress_bar.mark_as_completed();
        }

        m_temp_file.reset();
        m_valid_cache_path = m_writable_pkgs_dir;
        m_json_cache_valid = true;
        m_loaded = true;

        return true;
    }

    void MSubdirData::create_target()
    {
        assert(p_context != nullptr);

        auto& ctx = *p_context;
        fs::u8path writable_cache_dir = create_cache_dir(m_writable_pkgs_dir);
        auto lock = LockFile(writable_cache_dir);
        m_temp_file = std::make_unique<TemporaryFile>("mambaf", "", writable_cache_dir);

        bool use_zst = m_metadata.has_zst();
        m_target = std::make_unique<DownloadTarget>(
            ctx,
            m_name,
            m_repodata_url + (use_zst ? ".zst" : ""),
            m_temp_file->path().string()
        );
        if (!(ctx.graphics_params.no_progress_bars || ctx.output_params.quiet
              || ctx.output_params.json))
        {
            m_progress_bar = Console::instance().add_progress_bar(m_name);
            m_target->set_progress_bar(m_progress_bar);
        }
        // if we get something _other_ than the noarch, we DO NOT throw if the file can't be
        // retrieved
        if (!m_is_noarch)
        {
            m_target->set_ignore_failure(true);
        }
        m_target->set_finalize_callback(&MSubdirData::finalize_transfer, this);
        m_target->set_mod_etag_headers(m_metadata.mod(), m_metadata.etag());
    }

    std::size_t MSubdirData::get_cache_control_max_age(const std::string& val)
    {
        static std::regex max_age_re("max-age=(\\d+)");
        std::smatch max_age_match;
        bool matches = std::regex_search(val, max_age_match, max_age_re);
        if (!matches)
        {
            return 0;
        }
        return static_cast<std::size_t>(std::stoi(max_age_match[1]));
    }

    std::string cache_fn_url(const std::string& url)
    {
        return util::cache_name_from_url(url) + ".json";
    }

    std::string create_cache_dir(const fs::u8path& cache_path)
    {
        const auto cache_dir = cache_path / "cache";
        fs::create_directories(cache_dir);
#ifndef _WIN32
        ::chmod(cache_dir.string().c_str(), 02775);
#endif
        return cache_dir.string();
    }

    expected_t<MRepo> MSubdirData::create_repo(MPool& pool)
    {
        assert(&pool.context() == p_context);
        using return_type = expected_t<MRepo>;
        RepoMetadata meta{
            /* .url= */ util::rsplit(m_metadata.url(), "/", 1).front(),
            /* .etag= */ m_metadata.etag(),
            /* .mod= */ m_metadata.mod(),
            /* .pip_added= */ p_context->add_pip_as_python_dependency,
        };

        auto cache = cache_path();
        return cache ? return_type(MRepo(pool, m_name, *cache, meta))
                     : return_type(forward_error(cache));
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
