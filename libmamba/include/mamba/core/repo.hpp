// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_REPO_HPP
#define MAMBA_CORE_REPO_HPP

#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json_fwd.hpp>
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
        std::string url = {};
        std::string etag = {};
        std::string mod = {};
        bool pip_added = false;
    };

    auto operator==(const RepoMetadata& lhs, const RepoMetadata& rhs) -> bool;
    auto operator!=(const RepoMetadata& lhs, const RepoMetadata& rhs) -> bool;

    void to_json(nlohmann::json& j, const RepoMetadata& m);
    void from_json(const nlohmann::json& j, RepoMetadata& p);

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
        MRepo(MRepo&&) = default;
        MRepo& operator=(const MRepo&) = delete;
        MRepo& operator=(MRepo&&) = default;

        void set_installed();
        void set_priority(int priority, int subpriority);
        void add_pip_as_python_dependency();

        const fs::u8path& index_file();

        Id id() const;
        std::string_view name() const;
        std::string_view url() const;
        Repo* repo() const;
        std::tuple<int, int> priority() const;
        std::size_t size() const;

        bool clear(bool reuse_ids = true);

    private:

        bool load_file(const fs::u8path& filename);
        bool read_json(const fs::u8path& filename);
        bool read_solv(const fs::u8path& filename);
        void write_solv(fs::u8path path);
        void add_package_info(const PackageInfo& pkg_info);
        void set_solvables_url(const std::string& repo_url);

        MPool m_pool;
        fs::u8path m_json_file = {};
        fs::u8path m_solv_file = {};

        RepoMetadata m_metadata = {};

        Repo* m_repo = nullptr;  // This is a view managed by libsolv pool
    };

}  // namespace mamba

#endif  // MAMBA_REPO_HPP
