// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <string_view>
#include <tuple>

#include <simdjson.h>
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
#include "mamba/core/output.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/string.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"


#define MAMBA_TOOL_VERSION "2.0"

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

        void
        set_solvable(solv::ObjPool& pool, solv::ObjSolvableView solv, const specs::PackageInfo& pkg)
        {
            solv.set_name(pkg.name);
            solv.set_version(pkg.version);
            solv.set_build_string(pkg.build_string);
            if (pkg.noarch != specs::NoArchType::No)
            {
                auto noarch = std::string(specs::noarch_name(pkg.noarch));  // SSO
                solv.set_noarch(noarch);
            }
            solv.set_build_number(pkg.build_number);
            solv.set_channel(pkg.channel);
            solv.set_url(pkg.package_url);
            solv.set_subdir(pkg.subdir);
            solv.set_file_name(pkg.filename);
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
                const solv::DependencyId dep_id = pool.add_conda_dependency(dep);
                assert(dep_id);
                solv.add_dependency(dep_id);
            }

            for (const auto& cons : pkg.constrains)
            {
                // TODO pool's matchspec2id
                const solv::DependencyId dep_id = pool.add_conda_dependency(cons);
                assert(dep_id);
                solv.add_constraint(dep_id);
            }

            solv.add_track_features(pkg.track_features);

            solv.add_self_provide();
        }

        auto lsplit_track_features(std::string_view features)
        {
            constexpr auto is_sep = [](char c) -> bool { return (c == ',') || util::is_space(c); };
            auto [_, tail] = util::lstrip_if_parts(features, is_sep);
            return util::lstrip_if_parts(tail, [&](char c) { return !is_sep(c); });
        }

        [[nodiscard]] auto set_solvable(
            solv::ObjPool& pool,
            const std::string& repo_url_str,
            const specs::CondaURL& repo_url,
            solv::ObjSolvableView solv,
            const std::string& filename,
            const simdjson::dom::element& pkg,
            const std::string& default_subdir
        ) -> bool
        {
            // Not available from RepoDataPackage
            solv.set_file_name(filename);
            solv.set_url((repo_url / filename).str(specs::CondaURL::Credentials::Show));
            solv.set_channel(repo_url_str);

            if (auto name = pkg["name"].get_string(); !name.error())
            {
                solv.set_name(name.value_unsafe());
            }
            else
            {
                LOG_WARNING << R"(Found invalid name in ")" << filename << R"(")";
                return false;
            }

            if (auto version = pkg["version"].get_string(); !version.error())
            {
                solv.set_version(version.value_unsafe());
            }
            else
            {
                LOG_WARNING << R"(Found invalid version in ")" << filename << R"(")";
                return false;
            }

            if (auto build_string = pkg["build"].get_string(); !build_string.error())
            {
                solv.set_build_string(build_string.value_unsafe());
            }
            else
            {
                LOG_WARNING << R"(Found invalid build in ")" << filename << R"(")";
                return false;
            }

            if (auto build_number = pkg["build_number"].get_uint64(); !build_number.error())
            {
                solv.set_build_number(build_number.value_unsafe());
            }
            else
            {
                LOG_WARNING << R"(Found invalid build_number in ")" << filename << R"(")";
                return false;
            }

            if (auto subdir = pkg["subdir"].get_c_str(); !subdir.error())
            {
                solv.set_subdir(subdir.value_unsafe());
            }
            else
            {
                solv.set_subdir(default_subdir);
            }

            if (auto size = pkg["size"].get_uint64(); !size.error())
            {
                solv.set_size(size.value_unsafe());
            }

            if (auto md5 = pkg["md5"].get_c_str(); !md5.error())
            {
                solv.set_md5(md5.value_unsafe());
            }

            if (auto sha256 = pkg["sha256"].get_c_str(); !sha256.error())
            {
                solv.set_sha256(sha256.value_unsafe());
            }

            if (auto elem = pkg["noarch"]; !elem.error())
            {
                if (auto val = elem.get_bool(); !val.error() && val.value_unsafe())
                {
                    solv.set_noarch("generic");
                }
                else if (auto noarch = elem.get_c_str(); !noarch.error())
                {
                    solv.set_noarch(noarch.value_unsafe());
                }
            }

            if (auto license = pkg["license"].get_c_str(); !license.error())
            {
                solv.set_license(license.value_unsafe());
            }

            // TODO conda timestamp are not Unix timestamp.
            // Libsolv normalize them this way, we need to do the same here otherwise the current
            // package may get arbitrary priority.
            if (auto timestamp = pkg["timestamp"].get_uint64(); !timestamp.error())
            {
                const auto time = timestamp.value_unsafe();
                solv.set_timestamp((time > 253402300799ULL) ? (time / 1000) : time);
            }

            if (auto depends = pkg["depends"].get_array(); !depends.error())
            {
                for (auto elem : depends)
                {
                    if (auto dep = elem.get_c_str(); !dep.error())
                    {
                        if (const auto dep_id = pool.add_conda_dependency(dep.value_unsafe()))
                        {
                            solv.add_dependency(dep_id);
                        }
                    }
                }
            }

            if (auto constrains = pkg["constrains"].get_array(); !constrains.error())
            {
                for (auto elem : constrains)
                {
                    if (auto cons = elem.get_c_str(); !cons.error())
                    {
                        if (const auto dep_id = pool.add_conda_dependency(cons.value_unsafe()))
                        {
                            solv.add_constraint(dep_id);
                        }
                    }
                }
            }

            if (auto obj = pkg["track_features"]; !obj.error())
            {
                if (auto track_features_arr = obj.get_array(); !track_features_arr.error())
                {
                    for (auto elem : track_features_arr)
                    {
                        if (auto feat = elem.get_string(); !feat.error())
                        {
                            solv.add_track_feature(feat.value_unsafe());
                        }
                    }
                }
                else if (auto track_features_str = obj.get_string(); !track_features_str.error())
                {
                    auto splits = lsplit_track_features(track_features_str.value_unsafe());
                    while (!splits[0].empty())
                    {
                        solv.add_track_feature(splits[0]);
                        splits = lsplit_track_features(splits[1]);
                    }
                }
            }

            solv.add_self_provide();
            return true;
        }

        void set_repo_solvables(
            solv::ObjPool& pool,
            solv::ObjRepoView repo,
            const std::string& repo_url_str,
            const specs::CondaURL& repo_url,
            const std::string& default_subdir,
            const simdjson::dom::object& packages
        )
        {
            std::string filename = {};
            for (const auto& [fn, pkg] : packages)
            {
                auto [id, solv] = repo.add_solvable();
                filename = fn;
                const bool parsed = set_solvable(
                    pool,
                    repo_url_str,
                    repo_url,
                    solv,
                    filename,
                    pkg,
                    default_subdir
                );
                if (parsed)
                {
                    LOG_DEBUG << "Adding package record to repo " << fn;
                }
                else
                {
                    repo.remove_solvable(id, /* reuse_id= */ true);
                    LOG_WARNING << "Failed to parse from repodata " << fn;
                }
            }
        }

        void set_solvables_url(solv::ObjRepoView repo, const std::string& repo_url)
        {
            // WARNING cannot call ``url()`` at this point because it has not been internalized.
            // Setting the channel url on where the solvable so that we can retrace
            // where it came from
            const auto url = specs::CondaURL::parse(repo_url);
            repo.for_each_solvable(
                [&](solv::ObjSolvableView s)
                {
                    // The solvable url, this is not set in libsolv parsing so we set it manually
                    // while we still rely on libsolv for parsing
                    // TODO
                    s.set_url((url / s.file_name()).str(specs::CondaURL::Credentials::Show));
                    // The name of the channel where it came from, may be different from repo name
                    // for instance with the installed repo
                    s.set_channel(repo_url);
                }
            );
        }

    }

    MRepo::MRepo(
        MPool& pool,
        const std::string& name,
        const fs::u8path& index,
        const RepoMetadata& metadata,
        RepodataParser parser,
        LibsolvCache use_cache
    )
        : m_pool(pool)
        , m_metadata(metadata)
    {
        auto [_, repo] = pool.pool().add_repo(name);
        m_repo = repo.raw();
        repo.set_url(m_metadata.url);
        load_file(index, parser, use_cache);
        repo.internalize();
    }

    MRepo::MRepo(MPool& pool, const std::string& name, const std::vector<specs::PackageInfo>& package_infos)
        : m_pool(pool)
    {
        auto [_, repo] = pool.pool().add_repo(name);
        m_repo = repo.raw();
        for (auto& info : package_infos)
        {
            LOG_INFO << "Adding package record to repo " << info.name;
            auto [id, solv] = srepo(*this).add_solvable();
            set_solvable(m_pool.pool(), solv, info);
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
            set_solvable(m_pool.pool(), solv, record);
        }

        if (pool.context().add_pip_as_python_dependency)
        {
            add_pip_as_python_dependency();
        }

        repo.internalize();
        pool.pool().set_installed_repo(repo_id);
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

    void add_pip_as_python_dependency(::Pool* pool, solv::ObjRepoView repo)
    {
        const solv::DependencyId python_id = pool_conda_matchspec(pool, "python");
        const solv::DependencyId pip_id = pool_conda_matchspec(pool, "pip");
        repo.for_each_solvable(
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
        const int flags = m_pool.context().use_only_tar_bz2 ? CONDA_ADD_USE_ONLY_TAR_BZ2 : 0;
        srepo(*this).legacy_read_conda_repodata(filename, flags);
    }

    void MRepo::mamba_read_json(const fs::u8path& filename)
    {
        LOG_INFO << "Reading repodata.json file " << filename << " for repo " << name()
                 << " using mamba";

        auto parser = simdjson::dom::parser();
        const auto repodata = parser.load(filename);

        // An override for missing package subdir is found in at the top level
        auto default_subdir = std::string();
        if (auto subdir = repodata.at_pointer("/info/subdir").get_string(); subdir.error())
        {
            default_subdir = std::string(subdir.value_unsafe());
        }

        const auto repo_url = specs::CondaURL::parse(m_metadata.url);

        if (auto pkgs = repodata["packages"].get_object(); !pkgs.error())
        {
            set_repo_solvables(
                m_pool.pool(),
                srepo(*this),
                m_metadata.url,
                repo_url,
                default_subdir,
                pkgs.value()
            );
        }

        if (auto pkgs = repodata["packages.conda"].get_object();
            !pkgs.error() && !m_pool.context().use_only_tar_bz2)
        {
            set_repo_solvables(
                m_pool.pool(),
                srepo(*this),
                m_metadata.url,
                repo_url,
                default_subdir,
                pkgs.value()
            );
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

        LOG_INFO << "Metadata from solv are valid, loading successful";
        return true;
    }

    void MRepo::load_file(const fs::u8path& filename, RepodataParser parser, LibsolvCache use_cache)
    {
        auto repo = srepo(*this);
        bool is_solv = filename.extension() == ".solv";

        fs::u8path solv_file = filename;
        solv_file.replace_extension("solv");
        fs::u8path json_file = filename;
        json_file.replace_extension("json");

        LOG_INFO << "Reading cache files '" << (filename.parent_path() / filename).string()
                 << ".*' for repo index '" << name() << "'";

        // .solv files are slower to load than simdjson on Windows
        if (!util::on_win && is_solv && (use_cache == LibsolvCache::Yes))
        {
            const auto lock = LockFile(solv_file);
            const bool read = read_solv(solv_file);
            if (read)
            {
                set_solvables_url(repo, m_metadata.url);
                return;
            }
        }

        const auto& ctx = m_pool.context();

        {
            const auto lock = LockFile(json_file);
            if ((parser == RepodataParser::Mamba)
                || (parser == RepodataParser::Automatic && ctx.experimental_repodata_parsing))
            {
                mamba_read_json(json_file);
            }
            else
            {
                libsolv_read_json(json_file);
                set_solvables_url(repo, m_metadata.url);
            }
        }

        // TODO move this to a more structured approach for repodata patching?
        if (ctx.add_pip_as_python_dependency)
        {
            add_pip_as_python_dependency();
        }

        if (!util::on_win && (name() != "installed"))
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
