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

#include "tl/expected.hpp"
#include "nlohmann/json.hpp"

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/fetch.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/util.hpp"


namespace decompress
{
    bool raw(const std::string& in, const std::string& out);
}

namespace mamba
{

    enum class subdirdata_error
    {
        unknown,
        load
    };

    /**
     * Represents a channel subdirectory (i.e. a platform)
     * packages index. Handles downloading of the index
     * from the server and cache generation as well.
     */
    class MSubdirData
    {
    public:
        template <class T>
        using expected = tl::expected<T, mamba_error<subdirdata_error>>;

        static expected<MSubdirData> create(const Channel& channel,
                                            const std::string& platform,
                                            const std::string& url,
                                            MultiPackageCache& caches,
                                            const std::string& repodata_fn = "repodata.json");

        ~MSubdirData() = default;

        MSubdirData(const MSubdirData&) = delete;
        MSubdirData& operator=(const MSubdirData&) = delete;

        MSubdirData(MSubdirData&&);
        MSubdirData& operator=(MSubdirData&&);

        // TODO return seconds as double
        fs::file_time_type::duration check_cache(const fs::path& cache_file,
                                                 const fs::file_time_type::clock::time_point& ref);
        bool loaded() const;

        bool forbid_cache();
        void clear_cache();

        std::string cache_path() const;
        const std::string& name() const;

        DownloadTarget* target();
        bool finalize_transfer();

        MRepo& create_repo(MPool& pool);

    private:
        MSubdirData(const Channel& channel,
                    const std::string& platform,
                    const std::string& url,
                    MultiPackageCache& caches,
                    const std::string& repodata_fn = "repodata.json");

        bool load(MultiPackageCache& caches);
        bool decompress();
        void create_target(nlohmann::json& mod_etag);
        std::size_t get_cache_control_max_age(const std::string& val);

        std::unique_ptr<DownloadTarget> m_target = nullptr;

        bool m_json_cache_valid = false;
        bool m_solv_cache_valid = false;

        fs::path m_valid_cache_path;
        fs::path m_expired_cache_path;
        fs::path m_writable_pkgs_dir;

        ProgressProxy m_progress_bar;

        bool m_loaded;
        bool m_download_complete;
        std::string m_repodata_url;
        std::string m_name;
        std::string m_json_fn;
        std::string m_solv_fn;
        bool m_is_noarch;
        nlohmann::json m_mod_etag;
        std::unique_ptr<TemporaryFile> m_temp_file;
        const Channel* p_channel = nullptr;
    };

    // Contrary to conda original function, this one expects a full url
    // (that is channel url + / + repodata_fn). It is not the
    // responsibility of this function to decide whether it should
    // concatenate base url and repodata depending on repodata value
    // and old behavior support.
    std::string cache_fn_url(const std::string& url);
    std::string create_cache_dir(const fs::path& cache_path);

}  // namespace mamba

#endif  // MAMBA_SUBDIRDATA_HPP
