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

        void add_pip_as_python_dependency(solv::ObjPool& pool, solv::ObjRepoView repo)
        {
            const solv::DependencyId python_id = pool.add_conda_dependency("python");
            const solv::DependencyId pip_id = pool.add_conda_dependency("pip");
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
            repo.set_pip_added(true);
        }

        void libsolv_read_json(solv::ObjRepoView repo, const fs::u8path& filename, bool only_tar_bz2)
        {
            LOG_INFO << "Reading repodata.json file " << filename << " for repo " << repo.name()
                     << " using libsolv";
            // TODO make this as part of options of the repo/pool
            const int flags = only_tar_bz2 ? CONDA_ADD_USE_ONLY_TAR_BZ2 : 0;
            const auto lock = LockFile(filename);
            repo.legacy_read_conda_repodata(filename, flags);
        }

        void mamba_read_json(
            solv::ObjPool& pool,
            solv::ObjRepoView repo,
            const fs::u8path& filename,
            const std::string& repo_url,
            bool only_tar_bz2
        )
        {
            LOG_INFO << "Reading repodata.json file " << filename << " for repo " << repo.name()
                     << " using mamba";

            auto parser = simdjson::dom::parser();
            const auto lock = LockFile(filename);
            const auto repodata = parser.load(filename);

            // An override for missing package subdir is found in at the top level
            auto default_subdir = std::string();
            if (auto subdir = repodata.at_pointer("/info/subdir").get_string(); subdir.error())
            {
                default_subdir = std::string(subdir.value_unsafe());
            }

            const auto parsed_url = specs::CondaURL::parse(repo_url);

            if (auto pkgs = repodata["packages"].get_object(); !pkgs.error())
            {
                set_repo_solvables(pool, repo, repo_url, parsed_url, default_subdir, pkgs.value());
            }

            if (auto pkgs = repodata["packages.conda"].get_object(); !pkgs.error() && !only_tar_bz2)
            {
                set_repo_solvables(pool, repo, repo_url, parsed_url, default_subdir, pkgs.value());
            }
        }

        [[nodiscard]] auto read_solv(
            solv::ObjPool& pool,
            solv::ObjRepoView repo,
            const fs::u8path& filename,
            const solver::libsolv::RepodataOrigin& expected,
            bool expected_pip_added
        ) -> bool
        {
            using RepodataOrigin = solver::libsolv::RepodataOrigin;

            static constexpr auto expected_binary_version = std::string_view(MAMBA_SOLV_VERSION);

            LOG_INFO << "Attempting to read libsolv solv file " << filename << " for repo "
                     << repo.name();

            if (!fs::exists(filename))
            {
                LOG_INFO << "Solv file " << filename << " does not exist";
                return false;
            }

            {
                auto j = nlohmann::json(expected);
                j["tool_version"] = expected_binary_version;
                LOG_INFO << "Expecting solv metadata : " << j.dump();
            }

            {
                auto lock = LockFile(filename);
                repo.read(filename);
            }
            auto read_binary_version = repo.tool_version();

            if (read_binary_version != expected_binary_version)
            {
                LOG_INFO << "Metadata from solv are binary incompatible, canceling solv file load";
                repo.clear(/* reuse_ids= */ false);
                return false;
            }

            const auto read_metadata = RepodataOrigin{
                /* .url= */ std::string(repo.url()),
                /* .etag= */ std::string(repo.etag()),
                /* .mod= */ std::string(repo.mod()),
            };

            {
                auto j = nlohmann::json(read_metadata);
                j["tool_version"] = read_binary_version;
                LOG_INFO << "Loaded solv metadata : " << j.dump();
            }

            if ((read_metadata == RepodataOrigin{}) || (read_metadata != expected))
            {
                LOG_INFO << "Metadata from solv are outdated, canceling solv file load";
                repo.clear(/* reuse_ids= */ false);
                return false;
            }

            const bool read_pip_added = repo.pip_added();
            if (expected_pip_added != read_pip_added)
            {
                if (expected_pip_added)
                {
                    add_pip_as_python_dependency(pool, repo);
                    LOG_INFO << "Added missing pip dependencies";
                }
                else
                {
                    LOG_INFO << "Metadata from solv contain extra pip dependencies,"
                                " canceling solv file load";
                    repo.clear(/* reuse_ids= */ false);
                    return false;
                }
            }

            LOG_INFO << "Metadata from solv are valid, loading successful";
            return true;
        }

        void write_solv(
            solv::ObjRepoView repo,
            fs::u8path filename,
            const solver::libsolv::RepodataOrigin& metadata,
            const char* solv_bin_version
        )
        {
            LOG_INFO << "Writing libsolv solv file " << filename << " for repo " << repo.name();

            repo.set_url(metadata.url);
            repo.set_etag(metadata.etag);
            repo.set_mod(metadata.mod);
            repo.set_tool_version(solv_bin_version);
            repo.internalize();

            const auto lock = LockFile(fs::exists(filename) ? filename : filename.parent_path());
            repo.write(filename);
        }
    }

    MRepo::MRepo(
        MPool& pool,
        std::string_view name,
        const fs::u8path& index,
        const solver::libsolv::RepodataOrigin& metadata,
        PipAsPythonDependency add,
        RepodataParser parser,
        LibsolvCache use_cache
    )
    {
        auto [_, repo] = pool.pool().add_repo(name);
        m_repo = repo.raw();
        repo.set_url(metadata.url);
        load_file(pool, index, metadata, add, parser, use_cache);
        if (add == PipAsPythonDependency::Yes)
        {
            add_pip_as_python_dependency(pool.pool(), repo);
        }
        repo.internalize();
    }

    MRepo::MRepo(
        MPool& pool,
        const std::string_view name,
        const std::vector<specs::PackageInfo>& package_infos,
        PipAsPythonDependency add
    )
    {
        auto [_, repo] = pool.pool().add_repo(name);
        m_repo = repo.raw();
        for (auto& info : package_infos)
        {
            LOG_INFO << "Adding package record to repo " << info.name;
            auto [id, solv] = srepo(*this).add_solvable();
            set_solvable(pool.pool(), solv, info);
        }
        if (add == PipAsPythonDependency::Yes)
        {
            add_pip_as_python_dependency(pool.pool(), repo);
        }
        repo.internalize();
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

    void MRepo::load_file(
        MPool& pool,
        const fs::u8path& filename,
        const solver::libsolv::RepodataOrigin& metadata,
        PipAsPythonDependency add,
        RepodataParser parser,
        LibsolvCache use_cache
    )
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
            const bool read = read_solv(pool.pool(), repo, solv_file, metadata, static_cast<bool>(add));
            if (read)
            {
                set_solvables_url(repo, metadata.url);
                return;
            }
        }

        const auto& ctx = pool.context();

        if ((parser == RepodataParser::Mamba)
            || (parser == RepodataParser::Automatic && ctx.experimental_repodata_parsing))
        {
            mamba_read_json(pool.pool(), repo, json_file, metadata.url, pool.context().use_only_tar_bz2);
        }
        else
        {
            libsolv_read_json(repo, json_file, pool.context().use_only_tar_bz2);
            set_solvables_url(repo, metadata.url);
        }

        // TODO move this to a more structured approach for repodata patching?
        if (ctx.add_pip_as_python_dependency)
        {
            add_pip_as_python_dependency(pool.pool(), repo);
        }

        if (!util::on_win && (name() != "installed"))
        {
            write_solv(repo, solv_file, metadata, MAMBA_SOLV_VERSION);
        }
    }

    auto MRepo::py_name() const -> std::string_view
    {
        return name();
    }

    auto MRepo::py_priority() const -> std::tuple<int, int>
    {
        return std::make_tuple(m_repo->priority, m_repo->subpriority);
    }

    auto MRepo::py_size() const -> std::size_t
    {
        return srepo(*this).solvable_count();
    }
}  // namespace mamba
