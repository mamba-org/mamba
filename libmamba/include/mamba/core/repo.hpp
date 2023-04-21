// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_REPO_HPP
#define MAMBA_CORE_REPO_HPP

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <solv/pooltypes.h>

#include "mamba_fs.hpp"
#include "pool.hpp"

extern "C"
{
    typedef struct s_Repo Repo;
    typedef struct s_Repodata Repodata;
}

namespace mamba
{
    class PackageInfo;
    class PrefixData;

    /**
     * Represents a channel subdirectory
     * index.
     */
    struct RepoMetadata
    {
        std::string url;
        bool pip_added = false;
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

        MRepo(MPool& pool, const std::string& name, const std::string& filename, const std::string& url);
        MRepo(MPool& pool, const std::string& name, const fs::u8path& filename, const RepoMetadata& meta);
        MRepo(MPool& pool, const PrefixData& prefix_data);
        MRepo(MPool& pool, const std::string& name, const std::vector<PackageInfo>& uris);

        MRepo(const MRepo&) = delete;
        MRepo& operator=(const MRepo&) = delete;

        MRepo(MRepo&&);
        MRepo& operator=(MRepo&&);

        void set_installed();
        void set_priority(int priority, int subpriority);
        void add_pip_as_python_dependency();

        const fs::u8path& index_file();

        Id id() const;
        std::string name() const;
        bool write() const;
        const std::string& url() const;
        Repo* repo() const;
        std::tuple<int, int> priority() const;
        std::size_t size() const;

        bool clear(bool reuse_ids);

    private:

        void init();
        bool read_file(const fs::u8path& filename);
        void add_package_info(const PackageInfo& pkg_info);
        void set_solvables_url();

        MPool m_pool;
        fs::u8path m_json_file = {};
        fs::u8path m_solv_file = {};
        std::string m_url = {};

        RepoMetadata m_metadata = {};

        Repo* m_repo = nullptr;  // This is a view managed by libsolv pool
    };

}  // namespace mamba

#endif  // MAMBA_REPO_HPP
