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

#include "mamba/core/error_handling.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/subdir_parameters.hpp"
#include "mamba/core/util.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/platform.hpp"

namespace mamba
{
    namespace specs
    {
        class Channel;
    }

    class Context;
    class ChannelContext;

    /**
     * Handling of a subdirectory metadata.
     *
     * These metadata are used and stored to check if a subdirectory index is up to date,
     * where it comes from, and what protocols are supported to fetch it.
     */
    class SubdirMetadata
    {
    public:

        struct HttpMetadata
        {
            std::string url;
            std::string etag;
            std::string last_modified;
            std::string cache_control;
        };

        using expected_subdir_metadata = tl::expected<SubdirMetadata, mamba_error>;

        /** Read the metadata from a lightweight file containing only these metadata. */
        static auto read_state_file(  //
            const fs::u8path& state_file,
            const fs::u8path& repodata_file
        ) -> expected_subdir_metadata;

        /** Read the metadata from the ``repodata.json`` header. */
        static auto read_from_repodata_json(const fs::u8path& json) -> expected_subdir_metadata;

        /** Read from any of state file or ``repodata.json``depending on extension. */
        static auto read(const fs::u8path& file) -> expected_subdir_metadata;

        [[nodiscard]] auto is_valid_metadata(const fs::u8path& file) const -> bool;
        [[nodiscard]] auto url() const -> const std::string&;
        [[nodiscard]] auto etag() const -> const std::string&;
        [[nodiscard]] auto last_modified() const -> const std::string&;
        [[nodiscard]] auto cache_control() const -> const std::string&;

        /** Check if zst is available and freshly checked. */
        [[nodiscard]] auto has_up_to_date_zst() const -> bool;

        void set_http_metadata(HttpMetadata data);
        void set_zst(bool value);
        void store_file_metadata(const fs::u8path& file);

        /** Write the metadata to a lightweight file. */
        void write_state_file(const fs::u8path& file);

        friend void to_json(nlohmann::json& j, const SubdirMetadata& data);
        friend void from_json(const nlohmann::json& j, SubdirMetadata& data);

    private:

#ifdef _WIN32
        using time_type = std::chrono::system_clock::time_point;
#else
        using time_type = fs::file_time_type;
#endif

        struct CheckedAt
        {
            bool value;
            std::time_t last_checked;

            bool has_expired() const;
        };

        HttpMetadata m_http;
        std::optional<CheckedAt> m_has_zst;
        time_type m_stored_mtime;
        std::size_t m_stored_file_size;

        friend void to_json(nlohmann::json& j, const CheckedAt& ca);
        friend void from_json(const nlohmann::json& j, CheckedAt& ca);
    };

    /**
     * Channel sub-directory (i.e. a platform) packages index.
     *
     * Handles downloading of the index from the server and cache generation.
     * This only handle traditional ``repodata.json`` full index.
     * This abstraction does not load the index in memory, with is done by the @ref Database.
     *
     * Upon creation, the caches are checked for a valid and up to date index.
     * This can be inspected with @ref valid_cache_found.
     * The created subdirs are typically used with @ref SubdirData::download_required_indexes
     * which will download the missing, invalid, or outdaded indexes as needed.
     */
    class SubdirData
    {
    public:

        /**
         * Download the missing, invalid, or outdaded indexes as needed in parallel.
         *
         * It first creates check requests to update some metadata, then download the indexes.
         */
        [[nodiscard]] static auto download_required_indexes(
            std::vector<SubdirData>& subdirs,
            const Context& context,
            download::Monitor* check_monitor = nullptr,
            download::Monitor* download_monitor = nullptr
        ) -> expected_t<void>;

        /** Check existing caches for a valid index validity and freshness. */
        static auto create(
            const SubdirParams& params,
            ChannelContext& channel_context,
            const specs::Channel& channel,
            specs::DynamicPlatform platform,
            MultiPackageCache& caches,
            std::string repodata_filename = "repodata.json"
        ) -> expected_t<SubdirData>;

        [[nodiscard]] auto is_noarch() const -> bool;
        [[nodiscard]] auto name() const -> const std::string&;
        [[nodiscard]] auto channel_id() const -> const std::string&;
        [[nodiscard]] auto platform() const -> const specs::DynamicPlatform&;
        [[nodiscard]] auto metadata() const -> const SubdirMetadata&;

        [[nodiscard]] auto valid_cache_found() const -> bool;
        [[nodiscard]] auto valid_libsolv_cache_path() const -> expected_t<fs::u8path>;
        [[nodiscard]] auto writable_libsolv_cache_path() const -> fs::u8path;
        [[nodiscard]] auto valid_json_cache_path() const -> expected_t<fs::u8path>;

        void clear_cache_files();

        [[deprecated("since version 2.0 use ``valid_solv_cache`` or ``valid_json_cache`` instead")]]
        auto cache_path() const -> expected_t<std::string>;

    private:

        [[nodiscard]] static auto get_name(std::string_view channel_id, std::string_view platform)
            -> std::string;

        SubdirData(
            const SubdirParams& params,
            ChannelContext& channel_context,
            const specs::Channel& channel,
            std::string platform,
            MultiPackageCache& caches,
            std::string repodata_fn = "repodata.json"
        );

        [[nodiscard]] auto repodata_url_path() const -> std::string;
        [[nodiscard]] auto repodata_full_url() const -> const std::string&;

        void load(
            const MultiPackageCache& caches,
            ChannelContext& channel_context,
            const SubdirParams& params,
            const specs::Channel& channel
        );
        void load_cache(const MultiPackageCache& caches, const SubdirParams& params);
        void update_metadata_zst(
            ChannelContext& context,
            const SubdirParams& params,
            const specs::Channel& channel
        );

        auto build_check_requests(const SubdirParams& params) -> download::MultiRequest;
        auto build_index_request() -> download::Request;

        auto use_existing_cache() -> expected_t<void>;
        auto finalize_transfer(SubdirMetadata::HttpMetadata http_data) -> expected_t<void>;
        void refresh_last_write_time(const fs::u8path& json_file, const fs::u8path& solv_file);

        bool m_valid_cache_found = false;
        bool m_forbid_cache = false;
        bool m_json_cache_valid = false;
        bool m_solv_cache_valid = false;

        fs::u8path m_valid_cache_path;
        fs::u8path m_expired_cache_path;
        fs::u8path m_writable_pkgs_dir;

        specs::DynamicPlatform m_platform;
        std::string m_channel_id;
        std::string m_name;
        std::string m_repodata_fn;
        std::string m_json_filename;
        std::string m_solv_filename;

        std::string m_full_url;

        SubdirMetadata m_metadata;
        std::unique_ptr<TemporaryFile> m_temp_file;
    };

    /**
     * Compute an id from a URL.
     *
     * This is intended to keep unique, filesystem-safe, cache entries in the cache directory.
     * @see cache_filename_from_url
     */
    [[nodiscard]] auto cache_name_from_url(std::string url) -> std::string;

    /**
     * Compute a filename from a URL.
     *
     * This is intended to keep unique, filesystem-safe, cache entries in the cache directory.
     * Contrary to conda original function, this one expects a full url (that is
     * channel url + / + repodata_fn).
     * It is not the responsibility of this function to decide whether it should concatenate base
     * url and repodata depending on repodata value and old behavior support.
     */
    [[nodiscard]] auto cache_filename_from_url(std::string url) -> std::string;

    /**
     * Create cache directory with correct permissions
     *
     * @return The path to the directory created
     */
    auto create_cache_dir(const fs::u8path& cache_path) -> std::string;

}
#endif
