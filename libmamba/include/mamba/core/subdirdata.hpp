// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SUBDIRDATA_HPP
#define MAMBA_CORE_SUBDIRDATA_HPP

#include <memory>
#include <regex>
#include <string>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/core/fetch.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/util.hpp"

#include "package_handling.hpp"

namespace mamba
{
    class MSubdirMetadata
    {
    public:

        using expected_subdir_metadata = tl::expected<MSubdirMetadata, mamba_error>;

        static expected_subdir_metadata read(const fs::u8path& file);
        void write(const fs::u8path& file);
        bool check_valid_metadata(const fs::u8path& file);

        const std::string& url() const;
        const std::string& etag() const;
        const std::string& mod() const;
        const std::string& cache_control() const;

        bool has_zst() const;

        void store_http_metadata(std::string url, std::string etag, std::string mod, std::string cache_control);
        void store_file_metadata(const fs::u8path& file);
        void set_zst(bool value);

    private:

        static expected_subdir_metadata from_state_file(const fs::u8path& state_file, const fs::u8path& repodata_file);
        static expected_subdir_metadata from_repodata_file(const fs::u8path& json);

#ifdef _WIN32
        using time_type = std::chrono::system_clock::time_point;
#else
        using time_type = fs::file_time_type;
#endif

        std::string m_url;
        std::string m_etag;
        std::string m_mod;
        std::string m_cache_control;

        time_type m_stored_mtime;
        std::size_t m_stored_file_size;

        struct checked_at
        {
            bool value;
            std::time_t last_checked;

            bool has_expired() const;
        };

        std::optional<checked_at> m_has_zst;
    };

    /**
     * Represents a channel subdirectory (i.e. a platform)
     * packages index. Handles downloading of the index
     * from the server and cache generation as well.
     */
    class MSubdirData
    {
    public:

        static expected_t<MSubdirData> create(
            ChannelContext& channel_context,
            const Channel& channel,
            const std::string& platform,
            const std::string& url,
            MultiPackageCache& caches,
            const std::string& repodata_fn = "repodata.json"
        );

        ~MSubdirData() = default;

        MSubdirData(const MSubdirData&) = delete;
        MSubdirData& operator=(const MSubdirData&) = delete;

        MSubdirData(MSubdirData&&);
        MSubdirData& operator=(MSubdirData&&);

        // TODO return seconds as double
        fs::file_time_type::duration
        check_cache(const fs::u8path& cache_file, const fs::file_time_type::clock::time_point& ref) const;
        bool loaded() const;

        bool forbid_cache();
        void clear_cache();

        expected_t<std::string> cache_path() const;
        const std::string& name() const;

        std::vector<std::unique_ptr<DownloadTarget>>& check_targets();
        DownloadTarget* target();

        bool finalize_check(const DownloadTarget& target);
        bool finalize_transfer(const DownloadTarget& target);
        void finalize_checks();
        expected_t<MRepo> create_repo(MPool& pool);

    private:

        MSubdirData(
            ChannelContext& channel_context,
            const Channel& channel,
            const std::string& platform,
            const std::string& url,
            MultiPackageCache& caches,
            const std::string& repodata_fn = "repodata.json"
        );

        bool load(MultiPackageCache& caches, ChannelContext& channel_context);
        void check_repodata_existence();
        void create_target();
        std::size_t get_cache_control_max_age(const std::string& val);
        void refresh_last_write_time(const fs::u8path& json_file, const fs::u8path& solv_file);

        std::unique_ptr<DownloadTarget> m_target = nullptr;
        std::vector<std::unique_ptr<DownloadTarget>> m_check_targets;

        bool m_json_cache_valid = false;
        bool m_solv_cache_valid = false;

        fs::u8path m_valid_cache_path;
        fs::u8path m_expired_cache_path;
        fs::u8path m_writable_pkgs_dir;

        ProgressProxy m_progress_bar;
        ProgressProxy m_progress_bar_check;

        bool m_loaded;
        bool m_download_complete;
        std::string m_repodata_url;
        std::string m_name;
        std::string m_json_fn;
        std::string m_solv_fn;
        bool m_is_noarch;
        MSubdirMetadata m_metadata;
        std::unique_ptr<TemporaryFile> m_temp_file;
        const Channel* p_channel = nullptr;
        Context* p_context = nullptr;
    };

    // Contrary to conda original function, this one expects a full url
    // (that is channel url + / + repodata_fn). It is not the
    // responsibility of this function to decide whether it should
    // concatenate base url and repodata depending on repodata value
    // and old behavior support.
    std::string cache_fn_url(const std::string& url);
    std::string create_cache_dir(const fs::u8path& cache_path);

}  // namespace mamba

#endif  // MAMBA_SUBDIRDATA_HPP
