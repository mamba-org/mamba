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
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "mamba/core/pool.hpp"

extern "C"
{
    using Repo = struct s_Repo;
    using Repodata = struct s_Repodata;
}

namespace mamba
{

    namespace fs
    {
        class u8path;
    }

    namespace specs
    {
        class PackageInfo;
    }

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

        enum class RepodataParser
        {
            Automatic,
            Mamba,
            Libsolv,
        };

        enum class LibsolvCache : bool
        {
            No = false,
            Yes = true,
        };

        MRepo(
            MPool& pool,
            const std::string& name,
            const fs::u8path& filename,
            const RepoMetadata& meta,
            RepodataParser parser = RepodataParser::Automatic,
            LibsolvCache use_cache = LibsolvCache::Yes
        );
        MRepo(MPool& pool, const PrefixData& prefix_data);
        MRepo(MPool& pool, const std::string& name, const std::vector<specs::PackageInfo>& uris);

        MRepo(const MRepo&) = delete;
        MRepo(MRepo&&) = default;
        MRepo& operator=(const MRepo&) = delete;
        MRepo& operator=(MRepo&&) = default;

        void set_installed();
        void set_priority(int priority, int subpriority);

        Id id() const;
        Repo* repo() const;

        struct [[deprecated]] PyExtraPkgInfo
        {
            std::string noarch;
            std::string repo_url;
        };

        [[deprecated]] auto py_name() const -> std::string_view;
        [[deprecated]] auto py_priority() const -> std::tuple<int, int>;
        [[deprecated]] auto py_clear(bool reuse_ids) -> bool;
        [[deprecated]] auto py_size() const -> std::size_t;
        [[deprecated]] void
        py_add_extra_pkg_info(const std::map<std::string, PyExtraPkgInfo>& additional_info);

    private:

        auto name() const -> std::string_view;

        void add_pip_as_python_dependency();
        void clear(bool reuse_ids = true);
        void load_file(const fs::u8path& filename, RepodataParser parser, LibsolvCache use_cache);
        void libsolv_read_json(const fs::u8path& filename);
        void mamba_read_json(const fs::u8path& filename);
        bool read_solv(const fs::u8path& filename);
        void write_solv(fs::u8path path);

        MPool m_pool;

        RepoMetadata m_metadata = {};

        Repo* m_repo = nullptr;  // This is a view managed by libsolv pool
    };

}  // namespace mamba

#endif  // MAMBA_REPO_HPP
