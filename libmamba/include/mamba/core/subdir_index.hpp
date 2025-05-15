// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SUBDIRDATA_HPP
#define MAMBA_CORE_SUBDIRDATA_HPP

#include <algorithm>
#include <optional>
#include <string>
#include <type_traits>

#include <nlohmann/json_fwd.hpp>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/subdir_parameters.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/download/parameters.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/platform.hpp"

namespace mamba
{
    namespace specs
    {
        class Channel;
    }

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

        /** Read from any of state file or ``repodata.json`` depending on extension. */
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
     * This only handles traditional ``repodata.json`` full indexes.
     * This abstraction does not load the index in memory, with is done by the @ref Database.
     *
     * Upon creation, the caches are checked for a valid and up to date index.
     * This can be inspected with @ref valid_cache_found.
     * The created subdirs are typically used with @ref SubdirIndexLoader::download_required_indexes
     * which will download the missing, invalid, or outdated indexes as needed.
     */
    class SubdirIndexLoader
    {
    public:

        /**
         * Download the missing, invalid, or outdated indexes as needed in parallel.
         *
         * It first creates check requests to update some metadata, then download the indexes.
         * The result can be inspected with the input subdirs methods, such as
         * @ref valid_cache_found, @ref valid_json_cache_path etc.
         */
        template <typename SubdirIter1, typename SubdirIter2>
        [[nodiscard]] static auto download_required_indexes(
            SubdirIter1 subdirs_first,
            SubdirIter2 subdirs_last,
            const SubdirDownloadParams& subdir_params,
            const specs::AuthenticationDataBase& auth_info,
            const download::mirror_map& mirrors,
            const download::Options& download_options,
            const download::RemoteFetchParams& remote_fetch_params,
            download::Monitor* check_monitor = nullptr,
            download::Monitor* download_monitor = nullptr
        ) -> expected_t<void>;
        template <typename Subdirs>
        [[nodiscard]] static auto download_required_indexes(
            Subdirs& subdirs,
            const SubdirDownloadParams& subdir_params,
            const specs::AuthenticationDataBase& auth_info,
            const download::mirror_map& mirrors,
            const download::Options& download_options,
            const download::RemoteFetchParams& remote_fetch_params,
            download::Monitor* check_monitor = nullptr,
            download::Monitor* download_monitor = nullptr
        ) -> expected_t<void>;

        /** Check existing caches for a valid index validity and freshness. */
        static auto create(
            const SubdirParams& params,
            specs::Channel channel,
            specs::DynamicPlatform platform,
            MultiPackageCache& caches,
            std::string repodata_filename = "repodata.json"
        ) -> expected_t<SubdirIndexLoader>;

        [[nodiscard]] auto is_noarch() const -> bool;
        [[nodiscard]] auto is_local() const -> bool;
        [[nodiscard]] auto channel() const -> const specs::Channel&;
        [[nodiscard]] auto name() const -> std::string;
        [[nodiscard]] auto channel_id() const -> const std::string&;
        [[nodiscard]] auto platform() const -> const specs::DynamicPlatform&;
        [[nodiscard]] auto metadata() const -> const SubdirMetadata&;
        [[nodiscard]] auto repodata_url() const -> specs::CondaURL;

        [[nodiscard]] auto caching_is_forbidden() const -> bool;
        [[nodiscard]] auto valid_cache_found() const -> bool;
        [[nodiscard]] auto valid_libsolv_cache_path() const -> expected_t<fs::u8path>;
        [[nodiscard]] auto writable_libsolv_cache_path() const -> fs::u8path;
        [[nodiscard]] auto valid_json_cache_path() const -> expected_t<fs::u8path>;

        void clear_valid_cache_files();

    private:

        // This paths are pointing to what is found when iterating over the cache directories.
        // The expired found is the first one, which could be improved by keeping the freshest one.
        // This could improve caching in some HTTP 304 cases.
        // A possible improvement would be to keep all path, metadatas, and writable status in a
        // single vector and sort them by recency.
        // This would also give a public option for `clear`-ing all writable caches, not just the
        // valid one.
        SubdirMetadata m_metadata;
        specs::Channel m_channel;
        fs::u8path m_valid_cache_path;
        std::optional<fs::u8path> m_expired_cache_path;
        fs::u8path m_writable_pkgs_dir;
        specs::DynamicPlatform m_platform;
        std::string m_repodata_filename;
        std::string m_json_filename;
        std::string m_solv_filename;
        bool m_valid_cache_found = false;
        bool m_json_cache_valid = false;
        bool m_solv_cache_valid = false;

        SubdirIndexLoader(
            const SubdirParams& params,
            specs::Channel channel,
            std::string platform,
            MultiPackageCache& caches,
            std::string repodata_fn = "repodata.json"
        );

        [[nodiscard]] auto repodata_url_path() const -> std::string;
        [[nodiscard]] auto valid_json_cache_path_unchecked() const -> fs::u8path;
        [[nodiscard]] auto valid_state_file_path_unchecked() const -> fs::u8path;
        [[nodiscard]] auto valid_libsolv_cache_path_unchecked() const -> fs::u8path;

        /*********************************************************
         *  Implementation details of SubdirIndexLoader::create  *
         *********************************************************/

        void load(const MultiPackageCache& caches, const SubdirParams& params);
        void load_cache(const MultiPackageCache& caches, const SubdirParams& params);

        /****************************************************************************
         *  Implementation details of SubdirIndexLoader::download_required_indexes  *
         ****************************************************************************/

        auto use_existing_cache() -> expected_t<void>;
        auto finalize_transfer(SubdirMetadata::HttpMetadata http_data, const fs::u8path& artifact)
            -> expected_t<void>;
        void refresh_last_write_time(const fs::u8path& json_file, const fs::u8path& solv_file);

        template <typename First, typename End>
        static auto
        build_all_check_requests(First subdirs_first, End subdirs_last, const SubdirDownloadParams& params)
            -> download::MultiRequest;
        auto build_check_requests(const SubdirDownloadParams& params) -> download::MultiRequest;

        template <typename First, typename End>
        static auto
        build_all_index_requests(First subdirs_first, End subdirs_last, const SubdirDownloadParams& params)
            -> download::MultiRequest;
        auto build_index_request(const SubdirDownloadParams& params)
            -> std::optional<download::Request>;

        [[nodiscard]] static auto download_requests(
            download::MultiRequest index_requests,
            const specs::AuthenticationDataBase& auth_info,
            const download::mirror_map& mirrors,
            const download::Options& download_options,
            const download::RemoteFetchParams& remote_fetch_params,
            download::Monitor* download_monitor
        ) -> expected_t<void>;
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

    /*****************************************
     *  Implementation of SubdirIndexLoader  *
     *****************************************/

    template <typename SubdirIter1, typename SubdirIter2>
    auto SubdirIndexLoader::download_required_indexes(
        SubdirIter1 subdirs_first,
        SubdirIter2 subdirs_last,
        const SubdirDownloadParams& subdir_params,
        const specs::AuthenticationDataBase& auth_info,
        const download::mirror_map& mirrors,
        const download::Options& download_options,
        const download::RemoteFetchParams& remote_fetch_params,
        download::Monitor* check_monitor,
        download::Monitor* download_monitor
    ) -> expected_t<void>
    {
        auto result = download_requests(
            build_all_check_requests(subdirs_first, subdirs_last, subdir_params),
            auth_info,
            mirrors,
            download_options,
            remote_fetch_params,
            check_monitor
        );

        // Allow to continue if failed checks, unless asked to stop.
        constexpr auto is_interrupted = [](const auto& e)
        { return e.error_code() == mamba_error_code::user_interrupted; };

        if (!result.has_value() && result.map_error(is_interrupted).error())
        {
            return result;
        }

        return download_requests(
            build_all_index_requests(subdirs_first, subdirs_last, subdir_params),
            auth_info,
            mirrors,
            download_options,
            remote_fetch_params,
            download_monitor
        );
    }

    template <typename Subdirs>
    auto SubdirIndexLoader::download_required_indexes(
        Subdirs& subdirs,
        const SubdirDownloadParams& subdir_params,
        const specs::AuthenticationDataBase& auth_info,
        const download::mirror_map& mirrors,
        const download::Options& download_options,
        const download::RemoteFetchParams& remote_fetch_params,
        download::Monitor* check_monitor,
        download::Monitor* download_monitor
    ) -> expected_t<void>
    {
        return download_required_indexes(
            subdirs.begin(),
            subdirs.end(),
            subdir_params,
            auth_info,
            mirrors,
            download_options,
            remote_fetch_params,
            check_monitor,
            download_monitor
        );
    }

    template <typename First, typename End>
    auto SubdirIndexLoader::build_all_check_requests(
        First subdirs_first,
        End subdirs_last,
        const SubdirDownloadParams& params
    ) -> download::MultiRequest
    {
        download::MultiRequest requests;
        for (; subdirs_first != subdirs_last; ++subdirs_first)
        {
            // TODO(C++23): We make a special handling of iterators of pointers due to the
            // difficulty and necessity to create a range of references from Python objects.
            SubdirIndexLoader* p_subdir = nullptr;
            if constexpr (std::is_pointer_v<std::remove_reference_t<decltype(*subdirs_first)>>)
            {
                p_subdir = *subdirs_first;
            }
            else
            {
                p_subdir = &(*subdirs_first);
            }

            if (p_subdir != nullptr && !p_subdir->valid_cache_found())
            {
                auto check_list = p_subdir->build_check_requests(params);
                std::move(check_list.begin(), check_list.end(), std::back_inserter(requests));
            }
        }
        return requests;
    }

    template <typename First, typename End>
    auto SubdirIndexLoader::build_all_index_requests(
        First subdirs_first,
        End subdirs_last,
        const SubdirDownloadParams& params
    ) -> download::MultiRequest
    {
        download::MultiRequest requests;
        for (; subdirs_first != subdirs_last; ++subdirs_first)
        {
            // TODO(C++23): We make a special handling of iterators of pointers due to the
            // difficulty and necessity to create a range of references from Python objects.
            SubdirIndexLoader* p_subdir = nullptr;
            if constexpr (std::is_pointer_v<std::remove_reference_t<decltype(*subdirs_first)>>)
            {
                p_subdir = *subdirs_first;
            }
            else
            {
                p_subdir = &(*subdirs_first);
            }

            if (!p_subdir->valid_cache_found())
            {
                if (auto request = p_subdir->build_index_request(params))
                {
                    requests.push_back(*std::move(request));
                }
            }
        }
        return requests;
    }

}
#endif
