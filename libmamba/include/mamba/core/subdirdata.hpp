// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SUBDIRDATA_HPP
#define MAMBA_CORE_SUBDIRDATA_HPP

#include <memory>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/download.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"

namespace mamba
{
    class DownloadMonitor;

    class MSubdirMetadata
    {
    public:

        struct HttpMetadata
        {
            std::string url;
            std::string etag;
            std::string last_modified;
            std::string cache_control;
        };

        using expected_subdir_metadata = tl::expected<MSubdirMetadata, mamba_error>;

        static expected_subdir_metadata read(const fs::u8path& file);
        void write(const fs::u8path& file);
        bool check_valid_metadata(const fs::u8path& file);

        const std::string& url() const;
        const std::string& etag() const;
        const std::string& last_modified() const;
        const std::string& cache_control() const;

        bool has_zst() const;

        void store_http_metadata(HttpMetadata data);
        void store_file_metadata(const fs::u8path& file);
        void set_zst(bool value);

    private:

        static expected_subdir_metadata
        from_state_file(const fs::u8path& state_file, const fs::u8path& repodata_file);
        static expected_subdir_metadata from_repodata_file(const fs::u8path& json);

#ifdef _WIN32
        using time_type = std::chrono::system_clock::time_point;
#else
        using time_type = fs::file_time_type;
#endif

        HttpMetadata m_http;
        time_type m_stored_mtime;
        std::size_t m_stored_file_size;

        struct CheckedAt
        {
            bool value;
            std::time_t last_checked;

            bool has_expired() const;
        };

        std::optional<CheckedAt> m_has_zst;

        friend void to_json(nlohmann::json& j, const CheckedAt& ca);
        friend void from_json(const nlohmann::json& j, CheckedAt& ca);

        friend void to_json(nlohmann::json& j, const MSubdirMetadata& data);
        friend void from_json(const nlohmann::json& j, MSubdirMetadata& data);
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

        MSubdirData(MSubdirData&&) = default;
        MSubdirData& operator=(MSubdirData&&) = default;

        bool is_noarch() const;
        bool is_loaded() const;
        void clear_cache();

        const std::string& name() const;
        expected_t<std::string> cache_path() const;

        static expected_t<void> download_indexes(
            std::vector<MSubdirData>& subdirs,
            const Context& context,
            DownloadMonitor* check_monitor = nullptr,
            DownloadMonitor* download_monitor = nullptr
        );

        expected_t<MRepo> create_repo(MPool& pool) const;

    private:

        MSubdirData(
            ChannelContext& channel_context,
            const Channel& channel,
            const std::string& platform,
            const std::string& url,
            MultiPackageCache& caches,
            const std::string& repodata_fn = "repodata.json"
        );

        void load(MultiPackageCache& caches, ChannelContext& channel_context, const Channel& channel);
        void load_cache(MultiPackageCache& caches, ChannelContext& channel_context);
        void update_metadata_zst(ChannelContext& context, const Channel& channel);

        MultiDownloadRequest build_check_requests();
        DownloadRequest build_index_request();

        expected_t<void> use_existing_cache();
        expected_t<void> finalize_transfer(MSubdirMetadata::HttpMetadata http_data);
        void refresh_last_write_time(const fs::u8path& json_file, const fs::u8path& solv_file);

        bool m_loaded = false;
        bool m_json_cache_valid = false;
        bool m_solv_cache_valid = false;

        fs::u8path m_valid_cache_path;
        fs::u8path m_expired_cache_path;
        fs::u8path m_writable_pkgs_dir;

        std::string m_repodata_url;
        std::string m_name;
        std::string m_json_fn;
        std::string m_solv_fn;
        bool m_is_noarch;

        MSubdirMetadata m_metadata;
        std::unique_ptr<TemporaryFile> m_temp_file;
        Context* p_context;
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
