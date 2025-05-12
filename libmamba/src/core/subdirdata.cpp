// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <stdexcept>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/util/cryptography.hpp"
#include "mamba/util/json.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba
{
    /*******************
     * MSubdirMetadata *
     *******************/

    namespace
    {
#ifdef _WIN32
        std::chrono::system_clock::time_point filetime_to_unix(const fs::file_time_type& filetime)
        {
            // windows filetime is in 100ns intervals since 1601-01-01
            static constexpr auto epoch_offset = std::chrono::seconds(11644473600ULL);
            return std::chrono::system_clock::time_point(
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    filetime.time_since_epoch() - epoch_offset
                )
            );
        }
#endif

        // parse json at the beginning of the stream such as
        // "_url": "https://conda.anaconda.org/conda-forge/linux-64",
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

    void to_json(nlohmann::json& j, const SubdirMetadata::CheckedAt& ca)
    {
        j["value"] = ca.value;
        j["last_checked"] = timestamp(ca.last_checked);
    }

    void from_json(const nlohmann::json& j, SubdirMetadata::CheckedAt& ca)
    {
        int err_code = 0;
        ca.value = j["value"].get<bool>();
        ca.last_checked = parse_utc_timestamp(j["last_checked"].get<std::string>(), err_code);
    }

    void to_json(nlohmann::json& j, const SubdirMetadata& data)
    {
        j["url"] = data.m_http.url;
        j["etag"] = data.m_http.etag;
        j["mod"] = data.m_http.last_modified;
        j["cache_control"] = data.m_http.cache_control;
        j["size"] = data.m_stored_file_size;

        auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            data.m_stored_mtime.time_since_epoch()
        );
        j["mtime_ns"] = nsecs.count();
        j["has_zst"] = data.m_has_zst;
    }

    void from_json(const nlohmann::json& j, SubdirMetadata& data)
    {
        data.m_http.url = j["url"].get<std::string>();
        data.m_http.etag = j["etag"].get<std::string>();
        data.m_http.last_modified = j["mod"].get<std::string>();
        data.m_http.cache_control = j["cache_control"].get<std::string>();
        data.m_stored_file_size = j["size"].get<std::size_t>();

        using time_type = decltype(data.m_stored_mtime);
        data.m_stored_mtime = time_type(std::chrono::duration_cast<time_type::duration>(
            std::chrono::nanoseconds(j["mtime_ns"].get<std::size_t>())
        ));
        util::deserialize_maybe_missing(j, "has_zst", data.m_has_zst);
    }

    auto SubdirMetadata::read(const fs::u8path& file) -> expected_subdir_metadata
    {
        fs::u8path state_file = file;
        state_file.replace_extension(".state.json");
        if (fs::is_regular_file(state_file))
        {
            return read_state_file(state_file, file);
        }
        else
        {
            return read_from_repodata_json(file);
        }
    }

    void SubdirMetadata::write_state_file(const fs::u8path& file)
    {
        nlohmann::json j = *this;
        std::ofstream out = open_ofstream(file);
        out << j.dump(4);
    }

    bool SubdirMetadata::is_valid_metadata(const fs::u8path& file) const
    {
        if (const auto new_size = fs::file_size(file); new_size != m_stored_file_size)
        {
            LOG_INFO << "File size changed, expected " << m_stored_file_size << " but got "
                     << new_size << "; invalidating metadata";
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

    const std::string& SubdirMetadata::url() const
    {
        return m_http.url;
    }

    const std::string& SubdirMetadata::etag() const
    {
        return m_http.etag;
    }

    const std::string& SubdirMetadata::last_modified() const
    {
        return m_http.last_modified;
    }

    const std::string& SubdirMetadata::cache_control() const
    {
        return m_http.cache_control;
    }

    bool SubdirMetadata::has_up_to_date_zst() const
    {
        return m_has_zst.has_value() && m_has_zst.value().value && !m_has_zst.value().has_expired();
    }

    void SubdirMetadata::set_http_metadata(HttpMetadata data)
    {
        m_http = std::move(data);
    }

    void SubdirMetadata::store_file_metadata(const fs::u8path& file)
    {
#ifndef _WIN32
        m_stored_mtime = fs::last_write_time(file);
#else
        // convert windows filetime to unix timestamp
        m_stored_mtime = filetime_to_unix(fs::last_write_time(file));
#endif
        m_stored_file_size = fs::file_size(file);
    }

    void SubdirMetadata::set_zst(bool value)
    {
        m_has_zst = { value, std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
    }

    auto
    SubdirMetadata::read_state_file(const fs::u8path& state_file, const fs::u8path& repodata_file)
        -> expected_subdir_metadata
    {
        std::ifstream infile = open_ifstream(state_file);
        nlohmann::json j = nlohmann::json::parse(infile);
        SubdirMetadata m;
        try
        {
            m = j.get<SubdirMetadata>();
        }
        catch (const std::exception& e)
        {
            LOG_WARNING << "Could not parse state file: " << e.what();
            std::error_code ec;
            fs::remove(state_file, ec);
            if (ec)
            {
                LOG_WARNING << "Could not remove state file " << state_file << ": " << ec.message();
            }
            return make_unexpected(
                fmt::format("File: {}: Could not load cache state: {}", state_file, e.what()),
                mamba_error_code::cache_not_loaded
            );
        }

        if (!m.is_valid_metadata(repodata_file))
        {
            LOG_WARNING << "Cache file " << repodata_file << " was modified by another program";
            return make_unexpected(
                fmt::format("File: {}: Cache file mtime mismatch", state_file),
                mamba_error_code::cache_not_loaded
            );
        }
        return m;
    }

    auto SubdirMetadata::read_from_repodata_json(const fs::u8path& repodata_file)
        -> expected_subdir_metadata
    {
        const std::string json = [](const fs::u8path& file) -> std::string
        {
            auto lock = LockFile(file);
            std::ifstream in_file = open_ifstream(file);
            return extract_subjson(in_file);
        }(repodata_file);

        try
        {
            nlohmann::json result = nlohmann::json::parse(json);
            SubdirMetadata m;
            m.m_http.url = result.value("_url", "");
            m.m_http.etag = result.value("_etag", "");
            m.m_http.last_modified = result.value("_mod", "");
            m.m_http.cache_control = result.value("_cache_control", "");
            return m;
        }
        catch (std::exception& e)
        {
            LOG_DEBUG << "Could not parse mod/etag header";
            return make_unexpected(
                fmt::format("File: {}: Could not parse mod/etag header ({})", repodata_file, e.what()),
                mamba_error_code::cache_not_loaded
            );
        }
    }

    bool SubdirMetadata::CheckedAt::has_expired() const
    {
        // difference in seconds, check every 14 days
        constexpr double expiration = 60 * 60 * 24 * 14;
        return std::difftime(std::time(nullptr), last_checked) > expiration;
    }

    /***************
     * MSubdirData *
     ***************/

    namespace
    {
        using file_duration = fs::file_time_type::duration;
        using file_time_point = fs::file_time_type::clock::time_point;

        template <typename T>
        std::vector<T> without_duplicates(std::vector<T>&& values)
        {
            const auto end_it = std::unique(values.begin(), values.end());
            values.erase(end_it, values.end());
            return values;
        }

        file_duration get_cache_age(const fs::u8path& cache_file, const file_time_point& ref)
        {
            try
            {
                fs::file_time_type last_write = fs::last_write_time(cache_file);
                file_duration tdiff = ref - last_write;
                return tdiff;
            }
            catch (std::exception&)
            {
                // could not open the file...
                return file_duration::max();
            }
        }

        bool is_valid(const file_duration& age)
        {
            return age != file_duration::max();
        }

        int get_max_age(const std::string& cache_control, int local_repodata_ttl)
        {
            int max_age = local_repodata_ttl;
            if (local_repodata_ttl == 1)
            {
                static std::regex max_age_re("max-age=(\\d+)");
                std::smatch max_age_match;
                bool matches = std::regex_search(cache_control, max_age_match, max_age_re);
                if (!matches)
                {
                    max_age = 0;
                }
                else
                {
                    max_age = std::stoi(max_age_match[1]);
                }
            }
            return max_age;
        }

        fs::u8path get_cache_dir(const fs::u8path& cache_path)
        {
            return cache_path / "cache";
        }

        const fs::u8path& replace_file(const fs::u8path& old_file, const fs::u8path& new_file)
        {
            if (fs::is_regular_file(old_file))
            {
                fs::remove(old_file);
            }
            fs::copy(new_file, old_file);
            return old_file;
        }

        [[nodiscard]] auto get_name(std::string_view channel_id, std::string_view platform)
            -> std::string
        {
            return util::url_concat(channel_id, "/", platform);
        }

    }

    expected_t<SubdirData> SubdirData::create(
        const SubdirParams& params,
        ChannelContext& channel_context,
        specs::Channel channel,
        specs::DynamicPlatform platform,
        MultiPackageCache& caches,
        std::string repodata_filename
    )
    {
        if (channel.is_package())
        {
            return make_unexpected(
                "Channel pointing to a single package artifacts do not have an index.",
                mamba_error_code::incorrect_usage
            );
        }

        auto name = get_name(channel.id(), platform);
        try
        {
            return SubdirData(
                params,
                channel_context,
                std::move(channel),
                std::move(platform),
                caches,
                std::move(repodata_filename)
            );
        }
        catch (std::exception& e)
        {
            return make_unexpected(e.what(), mamba_error_code::subdirdata_not_loaded);
        }
        catch (...)
        {
            return make_unexpected(
                "Unknown error when trying to load subdir data " + name,
                mamba_error_code::unknown
            );
        }
    }

    bool SubdirData::is_noarch() const
    {
        return specs::platform_is_noarch(m_platform);
    }

    auto SubdirData::is_local() const -> bool
    {
        return (channel().mirror_urls().size() == 1u) && (channel().url().scheme() == "file");
    }

    auto SubdirData::channel() const -> const specs::Channel&
    {
        return m_channel;
    }

    auto SubdirData::caching_is_forbidden() const -> bool
    {
        // The only condition yet
        return is_local();
    }

    bool SubdirData::valid_cache_found() const
    {
        return m_valid_cache_found;
    }

    void SubdirData::clear_cache_files()
    {
        if (fs::is_regular_file(m_json_filename))
        {
            fs::remove(m_json_filename);
        }
        if (fs::is_regular_file(m_solv_filename))
        {
            fs::remove(m_solv_filename);
        }
    }

    std::string SubdirData::name() const
    {
        return get_name(channel_id(), m_platform);
    }

    const std::string& SubdirData::channel_id() const
    {
        return m_channel.id();
    }

    const specs::DynamicPlatform& SubdirData::platform() const
    {
        return m_platform;
    }

    const SubdirMetadata& SubdirData::metadata() const
    {
        return m_metadata;
    }

    expected_t<fs::u8path> SubdirData::valid_libsolv_cache_path() const
    {
        if (m_json_cache_valid && m_solv_cache_valid)
        {
            return (get_cache_dir(m_valid_cache_path) / m_solv_filename).string();
        }
        return make_unexpected("Cache not loaded", mamba_error_code::cache_not_loaded);
    }

    fs::u8path SubdirData::writable_libsolv_cache_path() const
    {
        return m_writable_pkgs_dir / "cache" / m_solv_filename;
    }

    expected_t<fs::u8path> SubdirData::valid_json_cache_path() const
    {
        if (m_json_cache_valid)
        {
            return (get_cache_dir(m_valid_cache_path) / m_json_filename).string();
        }
        return make_unexpected("Cache not loaded", mamba_error_code::cache_not_loaded);
    }

    expected_t<void> SubdirData::download_required_indexes(
        std::vector<SubdirData>& subdirs,
        const SubdirParams& subdir_params,
        const specs::AuthenticationDataBase& auth_info,
        const download::mirror_map& mirrors,
        const download::Options& download_options,
        const download::RemoteFetchParams& remote_fetch_params,
        download::Monitor* check_monitor,
        download::Monitor* download_monitor
    )
    {
        download::MultiRequest check_requests;
        for (auto& subdir : subdirs)
        {
            if (!subdir.valid_cache_found())
            {
                download::MultiRequest check_list = subdir.build_check_requests(subdir_params);
                std::move(check_list.begin(), check_list.end(), std::back_inserter(check_requests));
            }
        }
        download::download(
            std::move(check_requests),
            mirrors,
            remote_fetch_params,
            auth_info,
            download_options,
            check_monitor
        );

        if (is_sig_interrupted())
        {
            return make_unexpected("Interrupted by user", mamba_error_code::user_interrupted);
        }

        // TODO load local channels even when offline if (!ctx.offline)
        if (!subdir_params.offline)
        {
            download::MultiRequest index_requests;
            for (auto& subdir : subdirs)
            {
                if (!subdir.valid_cache_found())
                {
                    index_requests.push_back(subdir.build_index_request());
                }
            }

            try
            {
                download::download(
                    std::move(index_requests),
                    mirrors,
                    remote_fetch_params,
                    auth_info,
                    download_options,
                    download_monitor
                );
            }
            catch (const std::runtime_error& e)
            {
                return make_unexpected(e.what(), mamba_error_code::repodata_not_loaded);
            }
        }

        return expected_t<void>();
    }

    SubdirData::SubdirData(
        const SubdirParams& params,
        ChannelContext& channel_context,
        specs::Channel channel,
        std::string platform,
        MultiPackageCache& caches,
        std::string repodata_filename
    )
        : m_channel(std::move(channel))
        , m_valid_cache_path("")
        , m_expired_cache_path("")
        , m_writable_pkgs_dir(caches.first_writable_path())
        , m_platform(std::move(platform))
        , m_repodata_filename(std::move(repodata_filename))
        , m_json_filename(cache_filename_from_url(name()))
        , m_solv_filename(m_json_filename.substr(0, m_json_filename.size() - 4) + "solv")
    {
        assert(!this->channel().is_package());
        load(caches, channel_context, params, this->channel());
    }

    std::string SubdirData::repodata_url_path() const
    {
        return util::url_concat(m_platform, "/", m_repodata_filename);
    }

    specs::CondaURL SubdirData::repodata_url() const
    {
        return channel().platform_url(m_platform) / m_repodata_filename;
    }

    void SubdirData::load(
        const MultiPackageCache& caches,
        ChannelContext& channel_context,
        const SubdirParams& params,
        const specs::Channel& channel
    )
    {
        if (!caching_is_forbidden())
        {
            load_cache(caches, params);
        }

        if (m_valid_cache_found)
        {
            Console::stream() << fmt::format("{:<50} {:>20}", name(), std::string("Using cache"));
        }
        else
        {
            LOG_INFO << "No valid cache found";
            if (!m_expired_cache_path.empty())
            {
                LOG_INFO << "Expired cache (or invalid mod/etag headers) found at '"
                         << m_expired_cache_path.string() << "'";
            }
            update_metadata_zst(channel_context, params, channel);
        }
    }

    void SubdirData::load_cache(const MultiPackageCache& caches, const SubdirParams& params)
    {
        LOG_INFO << "Searching index cache file for repo '" << name() << "'";
        file_time_point now = fs::file_time_type::clock::now();

        const auto cache_paths = without_duplicates(caches.paths());

        for (const fs::u8path& cache_path : cache_paths)
        {
            // TODO: rewrite this with pipe chains of ranges
            fs::u8path json_file = cache_path / "cache" / m_json_filename;
            if (!fs::is_regular_file(json_file))
            {
                continue;
            }

            auto lock = LockFile(cache_path / "cache");
            file_duration cache_age = get_cache_age(json_file, now);
            if (!is_valid(cache_age))
            {
                continue;
            }

            auto metadata_temp = SubdirMetadata::read(json_file);
            if (!metadata_temp.has_value())
            {
                LOG_INFO << "Invalid json cache found, ignoring";
                continue;
            }
            m_metadata = std::move(metadata_temp.value());

            const int max_age = get_max_age(
                m_metadata.cache_control(),
                static_cast<int>(params.local_repodata_ttl)
            );
            const auto cache_age_seconds = std::chrono::duration_cast<std::chrono::seconds>(cache_age)
                                               .count();

            if ((max_age > cache_age_seconds || params.offline || params.use_index_cache))
            {
                // valid json cache found
                if (!m_valid_cache_found)
                {
                    LOG_DEBUG << "Using JSON cache";
                    LOG_TRACE << "Cache age: " << cache_age_seconds << "/" << max_age << "s";

                    m_valid_cache_path = cache_path;
                    m_json_cache_valid = true;
                    m_valid_cache_found = true;
                }

                // check libsolv cache
                fs::u8path solv_file = cache_path / "cache" / m_solv_filename;
                file_duration solv_age = get_cache_age(solv_file, now);

                if (is_valid(solv_age) && solv_age <= cache_age)
                {
                    // valid libsolv cache found
                    LOG_DEBUG << "Using SOLV cache";
                    LOG_TRACE << "Cache age: "
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
    }

    void SubdirData::update_metadata_zst(
        ChannelContext& channel_context,
        const SubdirParams& params,
        const specs::Channel& channel
    )
    {
        if (!params.offline || caching_is_forbidden())
        {
            m_metadata.set_zst(m_metadata.has_up_to_date_zst() || channel_context.has_zst(channel));
        }
    }

    download::MultiRequest SubdirData::build_check_requests(const SubdirParams& params)
    {
        download::MultiRequest request;

        if ((!params.offline || caching_is_forbidden()) && params.repodata_use_zst
            && !m_metadata.has_up_to_date_zst())
        {
            request.push_back(download::Request(
                name() + " (check zst)",
                download::MirrorName(channel_id()),
                repodata_url_path() + ".zst",
                "",
                /* lhead_only = */ true,
                /* lignore_failure = */ true
            ));

            request.back().on_success = [this](const download::Success& success)
            {
                const std::string& effective_url = success.transfer.effective_url;
                int http_status = success.transfer.http_status;
                LOG_INFO << "Checked: " << effective_url << " [" << http_status << "]";
                if (util::ends_with(effective_url, ".zst"))
                {
                    m_metadata.set_zst(http_status == 200);
                }
                return expected_t<void>();
            };

            request.back().on_failure = [this](const download::Error& error)
            {
                if (error.transfer.has_value())
                {
                    LOG_INFO << "Checked: " << error.transfer.value().effective_url << " ["
                             << error.transfer.value().http_status << "]";
                }
                m_metadata.set_zst(false);
            };
        }
        return request;
    }

    download::Request SubdirData::build_index_request()
    {
        fs::u8path writable_cache_dir = create_cache_dir(m_writable_pkgs_dir);
        auto lock = LockFile(writable_cache_dir);
        m_temp_file = std::make_unique<TemporaryFile>("mambaf", "", writable_cache_dir);

        bool use_zst = m_metadata.has_up_to_date_zst();

        download::Request request(
            name(),
            download::MirrorName(channel_id()),
            repodata_url_path() + (use_zst ? ".zst" : ""),
            m_temp_file->path().string(),
            /*head_only*/ false,
            /*ignore_failure*/ !is_noarch()
        );
        request.etag = m_metadata.etag();
        request.last_modified = m_metadata.last_modified();

        request.on_success = [this](const download::Success& success)
        {
            if (success.transfer.http_status == 304)
            {
                return use_existing_cache();
            }
            else
            {
                return finalize_transfer(SubdirMetadata::HttpMetadata{
                    repodata_url().str(),
                    success.etag,
                    success.last_modified,
                    success.cache_control,
                });
            }
        };

        request.on_failure = [](const download::Error& error)
        {
            if (error.transfer.has_value())
            {
                LOG_WARNING << "Unable to retrieve repodata (response: "
                            << error.transfer.value().http_status << ") for '"
                            << error.transfer.value().effective_url << "'";
            }
            else
            {
                LOG_WARNING << error.message;
            }
            if (error.retry_wait_seconds.has_value())
            {
                LOG_WARNING << "Retrying in " << error.retry_wait_seconds.value() << " seconds";
            }
        };

        return request;
    }

    expected_t<void> SubdirData::use_existing_cache()
    {
        LOG_INFO << "Cache is still valid";

        fs::u8path json_file = m_expired_cache_path / "cache" / m_json_filename;
        fs::u8path solv_file = m_expired_cache_path / "cache" / m_solv_filename;

        if (path::is_writable(json_file)
            && (!fs::is_regular_file(solv_file) || path::is_writable(solv_file)))
        {
            LOG_DEBUG << "Refreshing cache files ages";
            m_valid_cache_path = m_expired_cache_path;
        }
        else
        {
            if (m_writable_pkgs_dir.empty())
            {
                LOG_ERROR << "Could not find any writable cache directory for repodata file";
                return make_unexpected(
                    "Could not find any writable cache directory for repodata file",
                    mamba_error_code::subdirdata_not_loaded
                );
            }

            LOG_DEBUG << "Copying repodata cache files from '" << m_expired_cache_path.string()
                      << "' to '" << m_writable_pkgs_dir.string() << "'";
            fs::u8path writable_cache_dir = get_cache_dir(m_writable_pkgs_dir);
            auto lock = LockFile(writable_cache_dir);

            fs::u8path copied_json_file = writable_cache_dir / m_json_filename;
            json_file = replace_file(copied_json_file, json_file);

            if (fs::is_regular_file(solv_file))
            {
                auto copied_solv_file = writable_cache_dir / m_solv_filename;
                solv_file = replace_file(copied_solv_file, solv_file);
            }

            m_valid_cache_path = m_writable_pkgs_dir;
        }

        refresh_last_write_time(json_file, solv_file);

        m_temp_file.reset();
        m_valid_cache_found = true;
        return expected_t<void>();
    }

    expected_t<void> SubdirData::finalize_transfer(SubdirMetadata::HttpMetadata http_data)
    {
        if (m_writable_pkgs_dir.empty())
        {
            LOG_ERROR << "Could not find any writable cache directory for repodata file";
            return make_unexpected(
                "Could not find any writable cache directory for repodata file",
                mamba_error_code::subdirdata_not_loaded
            );
        }

        LOG_DEBUG << "Finalized transfer of '" << http_data.url << "'";

        m_metadata.set_http_metadata(std::move(http_data));

        fs::u8path writable_cache_dir = get_cache_dir(m_writable_pkgs_dir);
        fs::u8path json_file = writable_cache_dir / m_json_filename;
        auto lock = LockFile(writable_cache_dir);

        fs::u8path state_file = json_file;
        state_file.replace_extension(".state.json");
        std::error_code ec;
        mamba_fs::rename_or_move(m_temp_file->path(), json_file, ec);
        if (ec)
        {
            std::string error = fmt::format(
                "Could not move repodata file from {} to {}: {}",
                m_temp_file->path(),
                json_file,
                strerror(errno)
            );
            LOG_ERROR << error;
            return make_unexpected(error, mamba_error_code::subdirdata_not_loaded);
        }

        m_metadata.store_file_metadata(json_file);
        m_metadata.write_state_file(state_file);

        m_temp_file.reset();
        m_valid_cache_path = m_writable_pkgs_dir;
        m_json_cache_valid = true;
        m_valid_cache_found = true;

        return expected_t<void>();
    }

    void SubdirData::refresh_last_write_time(const fs::u8path& json_file, const fs::u8path& solv_file)
    {
        const auto now = fs::file_time_type::clock::now();

        file_duration json_age = get_cache_age(json_file, now);
        file_duration solv_age = get_cache_age(solv_file, now);

        {
            auto lock = LockFile(json_file);
            fs::last_write_time(json_file, fs::now());
            m_json_cache_valid = true;
        }

        if (fs::is_regular_file(solv_file) && solv_age.count() <= json_age.count())
        {
            auto lock = LockFile(solv_file);
            fs::last_write_time(solv_file, fs::now());
            m_solv_cache_valid = true;
        }

        fs::u8path state_file = json_file;
        state_file.replace_extension(".state.json");
        auto lock = LockFile(state_file);
        m_metadata.store_file_metadata(json_file);
        m_metadata.write_state_file(state_file);
    }

    std::string cache_name_from_url(std::string url)
    {
        if (url.empty() || (url.back() != '/' && !util::ends_with(url, ".json")))
        {
            url += '/';
        }

        // mimicking conda's behavior by special handling repodata.json
        // todo support .zst
        if (util::ends_with(url, "/repodata.json"))
        {
            url = url.substr(0, url.size() - 13);
        }
        return util::Md5Hasher().str_hex_str(url).substr(0, 8u);
    }

    std::string cache_filename_from_url(std::string url)
    {
        return cache_name_from_url(std::move(url)) + ".json";
    }

    std::string create_cache_dir(const fs::u8path& cache_path)
    {
        const auto cache_dir = cache_path / "cache";
        fs::create_directories(cache_dir);

        // Some filesystems don't support special permissions such as setgid on directories (e.g.
        // NFS). and fail if we try to set the setgid bit on the cache directory.
        //
        // We want to set the setgid bit on the cache directory to preserve the permissions as much
        // as possible if we can; hence we proceed in two steps to set the permissions by
        //   1. Setting the permissions without the setgid bit to the desired value without.
        //   2. Trying to set the setgid bit on the directory and report success or failure in log
        //   without raising an error or propagating an error which was raised.

        const auto permissions = fs::perms::owner_all | fs::perms::group_all
                                 | fs::perms::others_read | fs::perms::others_exec;
        fs::permissions(cache_dir, permissions, fs::perm_options::replace);
        LOG_TRACE << "Set permissions on cache directory " << cache_dir << " to 'rwxrwxr-x'";

        std::error_code ec;
        fs::permissions(cache_dir, fs::perms::set_gid, fs::perm_options::add, ec);

        if (!ec)
        {
            LOG_TRACE << "Set setgid bit on cache directory " << cache_dir;
        }
        else
        {
            LOG_TRACE << "Could not set setgid bit on cache directory " << cache_dir
                      << "\nReason:" << ec.message() << "; ignoring and continuing";
        }

        return cache_dir.string();
    }
}
