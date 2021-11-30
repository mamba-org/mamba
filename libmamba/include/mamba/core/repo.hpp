// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_REPO_HPP
#define MAMBA_CORE_REPO_HPP

#include <string>
#include <tuple>

#include "prefix_data.hpp"

extern "C"
{
#include "solv/conda.h"
#include "solv/repo.h"
#include "solv/repo_conda.h"
#include "solv/repo_solv.h"
}

#include "pool.hpp"

namespace mamba
{
    /**
     * Represents a channel subdirectory
     * index.
     */
    struct RepoMetadata
    {
        std::string url;
        bool pip_added;
        std::string etag;
        std::string mod;
    };

    inline bool operator==(const RepoMetadata& lhs, const RepoMetadata& rhs)
    {
        return lhs.url == rhs.url && lhs.pip_added == rhs.pip_added && lhs.etag == rhs.etag
               && lhs.mod == rhs.mod;
    }

    /**
     * A wrapper class of libsolv Repo.
     * Represents a channel subdirectory and
     * is built using a ready-to-use index/metadata
     * file (see ``MSubdirData``).
     */
    class MRepo
    {
    public:
        /**
         * Constructor.
         * @param pool ``libsolv`` pool wrapper
         * @param prefix_data prefix data
         */
        MRepo(MPool& pool, const PrefixData& prefix_data);

        /**
         * Constructor.
         * @param pool ``libsolv`` pool wrapper
         * @param name Name of the subdirectory (<channel>/<subdir>)
         * @param index Path to the index file
         * @param url Subdirectory URL
         */
        MRepo(MPool& pool,
              const std::string& name,
              const std::string& filename,
              const std::string& url);

        /**
         * Constructor.
         * @param pool ``libsolv`` pool wrapper
         * @param name Name of the subdirectory (<channel>/<subdir>)
         * @param index Path to the index file
         * @param meta Metadata of the repo
         */
        MRepo(MPool& pool,
              const std::string& name,
              const fs::path& filename,
              const RepoMetadata& meta);

        /**
         * Constructor.
         * @param pool ``libsolv`` pool wrapper
         * @param name Name
         * @param uris Matchspecs pointing to unique resources (URL or files)
         */
        MRepo(MPool& pool, const std::string& name, const std::vector<PackageInfo>& uris);

        ~MRepo();

        void set_installed();
        void set_priority(int priority, int subpriority);
        void add_package_info(Repodata*, const PackageInfo& pkg_info);
        void add_pip_as_python_dependency();

        const fs::path& index_file();

        std::string name() const;
        bool write() const;
        const std::string& url() const;
        Repo* repo();
        std::tuple<int, int> priority() const;
        std::size_t size() const;

        bool clear(bool reuse_ids);

    private:
        bool read_file(const fs::path& filename);

        fs::path m_json_file, m_solv_file;
        std::string m_url;

        RepoMetadata m_metadata;

        Repo* m_repo;
    };
}  // namespace mamba

#endif  // MAMBA_REPO_HPP
