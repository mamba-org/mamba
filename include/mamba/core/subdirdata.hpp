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

#include "nlohmann/json.hpp"

#include "context.hpp"
#include "fetch.hpp"
#include "mamba_fs.hpp"
#include "output.hpp"
#include "repo.hpp"
#include "util.hpp"


namespace decompress
{
    bool raw(const std::string& in, const std::string& out);
}

namespace mamba
{
    /**
     * Represents a channel subdirectory (i.e. a platform)
     * packages index. Handles downloading of the index
     * from the server and cache generation as well.
     */
    class MSubdirData
    {
    public:
        /**
         * Constructor.
         * @param name Name of the subdirectory (<channel>/<subdir>)
         * @param repodata_url URL of the repodata file
         * @param repodata_fn Local path of the repodata file
         */
        MSubdirData(const std::string& name,
                    const std::string& repodata_url,
                    const std::string& repodata_fn);

        // TODO return seconds as double
        fs::file_time_type::duration check_cache(const fs::path& cache_file,
                                                 const fs::file_time_type::clock::time_point& ref);
        bool load();
        bool loaded();

        bool forbid_cache();
        void clear_cache();

        std::string cache_path() const;
        const std::string& name() const;

        DownloadTarget* target();
        bool finalize_transfer();

        MRepo create_repo(MPool& pool);

    private:
        bool decompress();
        void create_target(nlohmann::json& mod_etag);
        std::size_t get_cache_control_max_age(const std::string& val);
        nlohmann::json read_mod_and_etag();

        std::unique_ptr<DownloadTarget> m_target;

        bool m_json_cache_valid = false;
        bool m_solv_cache_valid = false;

        std::ofstream out_file;

        ProgressProxy m_progress_bar;

        bool m_loaded;
        bool m_download_complete;
        std::string m_repodata_url;
        std::string m_name;
        std::string m_json_fn;
        std::string m_solv_fn;
        nlohmann::json m_mod_etag;
        std::unique_ptr<TemporaryFile> m_temp_file;
    };

    // Contrary to conda original function, this one expects a full url
    // (that is channel url + / + repodata_fn). It is not the
    // responsibility of this function to decide whether it should
    // concatenante base url and repodata depending on repodata value
    // and old behavior support.
    std::string cache_fn_url(const std::string& url);
    std::string create_cache_dir();

}  // namespace mamba

#endif  // MAMBA_SUBDIRDATA_HPP
