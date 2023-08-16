// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>
#include <fstream>
#include <tuple>

#include <nlohmann/json.hpp>
#include <solv/repo.h>
#include <solv/repo_solv.h>
#include <solv/repo_write.h>
#include <solv/solvable.h>
extern "C"  // Incomplete header
{
#include <solv/conda.h>
#include <solv/repo_conda.h>
}

#include "mamba/core/context.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/repo_data.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"


#define MAMBA_TOOL_VERSION "1.3"

#define MAMBA_SOLV_VERSION MAMBA_TOOL_VERSION "_" LIBSOLV_VERSION_STRING

namespace mamba
{
    namespace nl = nlohmann;

    namespace
    {
        auto attrs(const RepoMetadata& m)
        {
            return std::tie(m.url, m.etag, m.mod, m.pip_added);
        }
    }

    auto operator==(const RepoMetadata& lhs, const RepoMetadata& rhs) -> bool
    {
        return attrs(lhs) == attrs(rhs);
    }

    auto operator!=(const RepoMetadata& lhs, const RepoMetadata& rhs) -> bool
    {
        return !(lhs == rhs);
    }

    void to_json(nlohmann::json& j, const RepoMetadata& m)
    {
        j["url"] = m.url;
        j["etag"] = m.etag;
        j["mod"] = m.mod;
        j["pip_added"] = m.pip_added;
    }

    void from_json(const nlohmann::json& j, RepoMetadata& m)
    {
        m.url = j.value("url", m.url);
        m.etag = j.value("etag", m.etag);
        m.mod = j.value("mod", m.mod);
        m.pip_added = j.value("pip_added", m.pip_added);
    }

    namespace
    {
        // Keeping the solv-cpp header private
        auto srepo(const MRepo& r) -> solv::ObjRepoViewConst
        {
            return solv::ObjRepoViewConst{ *const_cast<const ::Repo*>(r.repo()) };
        }

        auto srepo(MRepo& r) -> solv::ObjRepoView
        {
            return solv::ObjRepoView{ *r.repo() };
        }

        void set_solvable(MPool& pool, solv::ObjSolvableView solv, const PackageInfo& pkg)
        {
            solv.set_name(pkg.name);
            solv.set_version(pkg.version);
            solv.set_build_string(pkg.build_string);
            solv.set_noarch(pkg.noarch);
            solv.set_build_number(pkg.build_number);
            solv.set_channel(pkg.channel);
            solv.set_url(pkg.url);
            solv.set_subdir(pkg.subdir);
            solv.set_file_name(pkg.fn);
            solv.set_license(pkg.license);
            solv.set_size(pkg.size);
            // TODO conda timestamp are not Unix timestamp.
            // Libsolv normalize them this way, we need to do the same here otherwise the current
            // package may get arbitrary priority.
            solv.set_timestamp(
                (pkg.timestamp > 253402300799ULL) ? (pkg.timestamp / 1000) : pkg.timestamp
            );
            solv.set_md5(pkg.md5);
            solv.set_sha256(pkg.sha256);

            for (const auto& dep : pkg.depends)
            {
                // TODO pool's matchspec2id
                solv::DependencyId const dep_id = pool_conda_matchspec(pool, dep.c_str());
                assert(dep_id);
                solv.add_dependency(dep_id);
            }

            for (const auto& cons : pkg.constrains)
            {
                // TODO pool's matchspec2id
                solv::DependencyId const dep_id = pool_conda_matchspec(pool, cons.c_str());
                assert(dep_id);
                solv.add_constraint(dep_id);
            }

            solv.add_track_features(pkg.track_features);

            solv.add_self_provide();
        }

        void set_solvable(
            MPool& pool,
            solv::ObjSolvableView solv,
            const std::string& filename,
            const specs::RepoDataPackage& pkg
        )
        {
            solv.set_name(pkg.name);
            solv.set_version(pkg.version.str());
            solv.set_build_string(pkg.build_string);
            // Un parse Noarch
            if (pkg.noarch == specs::NoArchType::Generic)
            {
                solv.set_noarch("generic");
            }
            else if (pkg.noarch == specs::NoArchType::Python)
            {
                solv.set_noarch("python");
            }
            solv.set_build_number(pkg.build_number);
            if (pkg.subdir)
            {
                solv.set_subdir(*pkg.subdir);
            }
            solv.set_file_name(filename);
            if (pkg.license)
            {
                solv.set_license(*pkg.license);
            }
            if (pkg.size)
            {
                solv.set_size(*pkg.size);
            }
            // TODO conda timestamp are not Unix timestamp.
            // Libsolv normalize them this way, we need to do the same here otherwise the current
            // package may get arbitrary priority.
            if (pkg.timestamp)
            {
                solv.set_timestamp(
                    (*pkg.timestamp > 253402300799ULL) ? (*pkg.timestamp / 1000) : *pkg.timestamp
                );
            }
            if (pkg.md5)
            {
                solv.set_md5(*pkg.md5);
            }
            if (pkg.sha256)
            {
                solv.set_sha256(*pkg.sha256);
            }

            for (const auto& dep : pkg.depends)
            {
                // TODO pool's matchspec2id
                solv::DependencyId const dep_id = pool_conda_matchspec(pool, dep.c_str());
                assert(dep_id);
                solv.add_dependency(dep_id);
            }

            for (const auto& cons : pkg.constrains)
            {
                // TODO pool's matchspec2id
                solv::DependencyId const dep_id = pool_conda_matchspec(pool, cons.c_str());
                assert(dep_id);
                solv.add_constraint(dep_id);
            }

            solv.add_track_features(pkg.track_features);

            solv.add_self_provide();
        }
    }

    MRepo::MRepo(
        MPool& pool,
        const std::string& name,
        const fs::u8path& index,
        const RepoMetadata& metadata,
        RepodataParser parser
    )
        : m_pool(pool)
        , m_metadata(metadata)
    {
        auto [_, repo] = pool.pool().add_repo(name);
        m_repo = repo.raw();
        repo.set_url(m_metadata.url);
        load_file(index, parser);
        repo.internalize();
    }

    MRepo::MRepo(MPool& pool, const std::string& name, const std::vector<PackageInfo>& package_infos)
        : m_pool(pool)
    {
        auto [_, repo] = pool.pool().add_repo(name);
        m_repo = repo.raw();
        for (auto& info : package_infos)
        {
            LOG_INFO << "Adding package record to repo " << info.name;
            auto [id, solv] = srepo(*this).add_solvable();
            set_solvable(m_pool, solv, info);
        }
        repo.internalize();
    }

    MRepo::MRepo(MPool& pool, const PrefixData& prefix_data)
        : m_pool(pool)
    {
        auto [repo_id, repo] = pool.pool().add_repo("installed");
        m_repo = repo.raw();

        for (auto& [name, record] : prefix_data.records())
        {
            LOG_INFO << "Adding package record to repo " << record.name;
            auto [id, solv] = srepo(*this).add_solvable();
            set_solvable(m_pool, solv, record);
        }

        if (Context::instance().add_pip_as_python_dependency)
        {
            add_pip_as_python_dependency();
        }

        repo.internalize();
        pool.pool().set_installed_repo(repo_id);
    }

    void MRepo::set_solvables_url(const std::string& repo_url)
    {
        // WARNING cannot call ``url()`` at this point because it has not been internalized.
        // Setting the channel url on where the solvable so that we can retrace
        // where it came from
        srepo(*this).for_each_solvable(
            [&](solv::ObjSolvableView s)
            {
                // The solvable url, this is not set in libsolv parsing so we set it manually
                // while we still rely on libsolv for parsing
                s.set_url(join_url(repo_url, s.file_name()));
                // The name of the channel where it came from, may be different from repo name
                // for instance with the installed repo
                s.set_channel(repo_url);
            }
        );
    }

    void MRepo::set_installed()
    {
        m_pool.pool().set_installed_repo(srepo(*this).id());
    }

    void MRepo::set_priority(int priority, int subpriority)
    {
        m_repo->priority = priority;
        m_repo->subpriority = subpriority;
    }

    auto MRepo::name() const -> std::string_view
    {
        return srepo(*this).name();
    }

    Id MRepo::id() const
    {
        return srepo(*this).id();
    }

    Repo* MRepo::repo() const
    {
        return m_repo;
    }

    void MRepo::add_pip_as_python_dependency()
    {
        solv::DependencyId const python_id = pool_conda_matchspec(m_pool, "python");
        solv::DependencyId const pip_id = pool_conda_matchspec(m_pool, "pip");
        srepo(*this).for_each_solvable(
            [&](solv::ObjSolvableView s)
            {
                if ((s.name() == "python") && !s.version().empty() && (s.version()[0] >= '2'))
                {
                    s.add_dependency(pip_id);
                }
                if (s.name() == "pip")
                {
                    s.add_dependency(python_id, SOLVABLE_PREREQMARKER);
                }
            }
        );
    }

    void MRepo::libsolv_read_json(const fs::u8path& filename)
    {
        LOG_INFO << "Reading repodata.json file " << filename << " for repo " << name()
                 << " using libsolv";
        // TODO make this as part of options of the repo/pool
        const int flags = Context::instance().use_only_tar_bz2 ? CONDA_ADD_USE_ONLY_TAR_BZ2 : 0;
        srepo(*this).legacy_read_conda_repodata(filename, flags);
        set_solvables_url(m_metadata.url);
    }

    void MRepo::mamba_read_json(const fs::u8path& filename)
    {
        LOG_INFO << "Reading repodata.json file " << filename << " for repo " << name()
                 << " using mamba";

        auto filename_stream = std::ifstream(filename.std_path());
        const auto repodata = nl::json::parse(filename_stream).get<specs::RepoData>();

        for (const auto& [fn, pkg] : repodata.packages)
        {
            LOG_INFO << "Adding package record to repo " << fn;
            auto [id, solv] = srepo(*this).add_solvable();
            set_solvable(m_pool, solv, fn, pkg);
        }

        if (!Context::instance().use_only_tar_bz2)
        {
            for (const auto& [fn, pkg] : repodata.conda_packages)
            {
                LOG_INFO << "Adding package record to repo " << fn;
                auto [id, solv] = srepo(*this).add_solvable();
                set_solvable(m_pool, solv, fn, pkg);
            }
        }
    }

    bool MRepo::read_solv(const fs::u8path& filename)
    {
        LOG_INFO << "Attempting to read libsolv solv file " << filename << " for repo " << name();

        auto repo = srepo(*this);

        auto lock = LockFile(filename);
        repo.read(filename);

        const auto read_metadata = RepoMetadata{
            /* .url= */ std::string(repo.url()),
            /* .etag= */ std::string(repo.etag()),
            /* .mod= */ std::string(repo.mod()),
            /* .pip_added= */ repo.pip_added(),
        };
        const auto tool_version = repo.tool_version();

        {
            auto j = nlohmann::json(m_metadata);
            j["tool_version"] = tool_version;
            LOG_INFO << "Expecting solv metadata : " << j.dump();
        }
        {
            auto j = nlohmann::json(read_metadata);
            j["tool_version"] = tool_version;
            LOG_INFO << "Loaded solv metadata : " << j.dump();
        }

        if ((tool_version != std::string_view(MAMBA_SOLV_VERSION))
            || (read_metadata == RepoMetadata{}) || (read_metadata != m_metadata))
        {
            LOG_INFO << "Metadata from solv are NOT valid, canceling solv file load";
            repo.clear(/* reuse_ids= */ false);
            return false;
        }

        set_solvables_url(m_metadata.url);
        LOG_INFO << "Metadata from solv are valid, loading successful";
        return true;
    }

    void MRepo::load_file(const fs::u8path& filename, RepodataParser parser)
    {
        auto repo = srepo(*this);
        bool is_solv = filename.extension() == ".solv";

        fs::u8path solv_file = filename;
        fs::u8path json_file = filename;

        if (is_solv)
        {
            json_file.replace_extension("json");
        }
        else
        {
            solv_file.replace_extension("solv");
        }

        LOG_INFO << "Reading cache files '" << (filename.parent_path() / filename).string()
                 << ".*' for repo index '" << name() << "'";

        if (is_solv)
        {
            const auto lock = LockFile(solv_file);
            const bool read = read_solv(solv_file);
            if (read)
            {
                return;
            }
        }

        auto lock = LockFile(json_file);
        const auto& ctx = Context::instance();

        if ((parser == RepodataParser::mamba)
            || (parser == RepodataParser::automatic && ctx.experimental_repodata_parsing))
        {
            mamba_read_json(json_file);
        }
        else
        {
            libsolv_read_json(json_file);
        }

        // TODO move this to a more structured approach for repodata patching?
        if (ctx.add_pip_as_python_dependency)
        {
            add_pip_as_python_dependency();
        }

        if (name() != "installed")
        {
            write_solv(solv_file);
        }
    }

    void MRepo::write_solv(fs::u8path filename)
    {
        LOG_INFO << "Writing libsolv solv file " << filename << " for repo " << name();

        auto repo = srepo(*this);
        repo.set_url(m_metadata.url);
        repo.set_etag(m_metadata.etag);
        repo.set_mod(m_metadata.mod);
        repo.set_pip_added(m_metadata.pip_added);
        repo.set_tool_version(MAMBA_SOLV_VERSION);
        repo.internalize();

        repo.write(filename);
    }

    void MRepo::clear(bool reuse_ids)
    {
        m_pool.remove_repo(id(), reuse_ids);
    }
}

#include <map>

namespace mamba
{

    auto MRepo::py_name() const -> std::string_view
    {
        return name();
    }

    auto MRepo::py_priority() const -> std::tuple<int, int>
    {
        return std::make_tuple(m_repo->priority, m_repo->subpriority);
    }

    auto MRepo::py_clear(bool reuse_ids) -> bool
    {
        clear(reuse_ids);
        return true;
    }

    auto MRepo::py_size() const -> std::size_t
    {
        return srepo(*this).solvable_count();
    }

    void MRepo::py_add_extra_pkg_info(const std::map<std::string, PyExtraPkgInfo>& extra)
    {
        auto repo = srepo(*this);

        repo.for_each_solvable(
            [&](solv::ObjSolvableView s)
            {
                if (auto const it = extra.find(std::string(s.name())); it != extra.cend())
                {
                    if (auto const& noarch = it->second.noarch; !noarch.empty())
                    {
                        s.set_noarch(noarch);
                    }
                    if (auto const& repo_url = it->second.repo_url; !repo_url.empty())
                    {
                        s.set_channel(repo_url);
                    }
                }
            }
        );
        repo.internalize();
    }
}  // namespace mamba
