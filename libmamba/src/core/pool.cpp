// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <exception>
#include <iostream>
#include <limits>
#include <string_view>

#include <fmt/format.h>
#include <solv/evr.h>
#include <solv/selection.h>
#include <solv/solver.h>
#include <spdlog/spdlog.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/random.hpp"
#include "mamba/util/string.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"

#include "solver/libsolv/helpers.hpp"

namespace mamba
{
    struct MPool::MPoolData
    {
        MPoolData(specs::ChannelResolveParams channel_params)
            : channel_params(std::move(channel_params))
        {
        }

        specs::ChannelResolveParams channel_params;
        solv::ObjPool pool = {};
    };

    MPool::MPool(specs::ChannelResolveParams channel_params)
        : m_data(std::make_unique<MPoolData>(std::move(channel_params)))
    {
        pool().set_disttype(DISTTYPE_CONDA);
        // Ensure that debug logging never goes to stdout as to not interfere json output
        pool().raw()->debugmask |= SOLV_DEBUG_TO_STDERR;
        ::pool_setdebuglevel(pool().raw(), -1);  // Off
    }

    MPool::~MPool() = default;

    auto MPool::channel_params() const -> const specs::ChannelResolveParams&
    {
        return m_data->channel_params;
    }

    solv::ObjPool& MPool::pool()
    {
        return m_data->pool;
    }

    const solv::ObjPool& MPool::pool() const
    {
        return m_data->pool;
    }

    namespace
    {
        auto libsolv_to_log_level(int type) -> solver::libsolv::LogLevel
        {
            if (type & SOLV_FATAL)
            {
                return solver::libsolv::LogLevel::Fatal;
            }
            if (type & SOLV_ERROR)
            {
                return solver::libsolv::LogLevel::Error;
            }
            if (type & SOLV_WARN)
            {
                return solver::libsolv::LogLevel::Warning;
            }
            return solver::libsolv::LogLevel::Debug;
        }
    }

    void MPool::set_logger(logger_type callback)
    {
        ::pool_setdebuglevel(pool().raw(), std::numeric_limits<int>::max());  // All
        pool().set_debug_callback(
            [logger = std::move(callback)](::Pool*, int type, std::string_view msg) noexcept
            {
                try
                {
                    logger(libsolv_to_log_level(type), msg);
                }
                catch (std::exception const& e)
                {
                    std::cerr << "Developer error: error in libsolv logging function: \n"
                              << e.what();
                }
            }
        );
    }

    void MPool::create_whatprovides()
    {
        pool().create_whatprovides();
    }

    std::vector<Id> MPool::select_solvables(Id matchspec, bool sorted) const
    {
        auto solvables = pool().select_solvables({ SOLVER_SOLVABLE_PROVIDES, matchspec });

        if (sorted)
        {
            std::sort(
                solvables.begin(),
                solvables.end(),
                [pool_ptr = pool().raw()](Id a, Id b)
                {
                    Solvable* sa = pool_id2solvable(pool_ptr, a);
                    Solvable* sb = pool_id2solvable(pool_ptr, b);
                    return (pool_evrcmp(pool_ptr, sa->evr, sb->evr, EVRCMP_COMPARE) > 0);
                }
            );
        }
        return solvables.as<std::vector>();
    }

    auto MPool::matchspec2id(const specs::MatchSpec& ms) -> ::Id
    {
        return solver::libsolv::pool_add_matchspec(pool(), ms, channel_params())
            .or_else([](mamba_error&& error) { throw std::move(error); })
            .value_or(0);
    }

    std::optional<specs::PackageInfo> MPool::id2pkginfo(Id solv_id) const
    {
        if (const auto solv = pool().get_solvable(solv_id))
        {
            return { solver::libsolv::make_package_info(pool(), solv.value()) };
        }
        return std::nullopt;
    }

    std::optional<std::string> MPool::dep2str(Id dep_id) const
    {
        if (!dep_id)
        {
            return std::nullopt;
        }
        // Not const because might alloctmp space
        return pool_dep2str(const_cast<::Pool*>(pool().raw()), dep_id);
    }

    auto MPool::add_repo_from_repodata_json(
        const fs::u8path& path,
        std::string_view url,
        solver::libsolv::PipAsPythonDependency add,
        solver::libsolv::UseOnlyTarBz2 only_tar,
        solver::libsolv::RepodataParser parser
    ) -> expected_t<solver::libsolv::RepoInfo>
    {
        const auto use_only_tar_bz2 = static_cast<bool>(only_tar);

        if (!fs::exists(path))
        {
            return make_unexpected(
                fmt::format(R"(File "{}" does not exist)", path),
                mamba_error_code::repodata_not_loaded
            );
        }
        auto repo = pool().add_repo(url).second;
        repo.set_url(std::string(url));

        auto make_repo = [&]() -> expected_t<solv::ObjRepoView>
        {
            if (parser == solver::libsolv::RepodataParser::Mamba)
            {
                return solver::libsolv::mamba_read_json(
                    pool(),
                    repo,
                    path,
                    std::string(url),
                    use_only_tar_bz2
                );
            }
            return solver::libsolv::libsolv_read_json(repo, path, use_only_tar_bz2)
                .transform(
                    [&url](solv::ObjRepoView repo)
                    {
                        solver::libsolv::set_solvables_url(repo, std::string(url));
                        return repo;
                    }
                );
        };

        return make_repo()
            .transform(
                [&](solv::ObjRepoView repo) -> solver::libsolv::RepoInfo
                {
                    if (add == solver::libsolv::PipAsPythonDependency::Yes)
                    {
                        solver::libsolv::add_pip_as_python_dependency(pool(), repo);
                    }
                    repo.internalize();
                    return solver::libsolv::RepoInfo{ repo.raw() };
                }
            )
            .or_else([&](const auto&) { pool().remove_repo(repo.id(), /* reuse_ids= */ true); });
    }

    auto MPool::add_repo_from_native_serialization(
        const fs::u8path& path,
        const solver::libsolv::RepodataOrigin& expected,
        solver::libsolv::PipAsPythonDependency add
    ) -> expected_t<solver::libsolv::RepoInfo>
    {
        auto repo = pool().add_repo(expected.url).second;

        return solver::libsolv::read_solv(pool(), repo, path, expected, static_cast<bool>(add))
            .transform(
                [&](solv::ObjRepoView repo) -> solver::libsolv::RepoInfo
                {
                    repo.set_url(expected.url);
                    solver::libsolv::set_solvables_url(repo, expected.url);
                    if (add == solver::libsolv::PipAsPythonDependency::Yes)
                    {
                        solver::libsolv::add_pip_as_python_dependency(pool(), repo);
                    }
                    repo.internalize();
                    return solver::libsolv::RepoInfo(repo.raw());
                }
            )
            .or_else([&](const auto&) { pool().remove_repo(repo.id(), /* reuse_ids= */ true); });
    }

    auto MPool::add_repo_from_packages_impl_pre(std::string_view name) -> solver::libsolv::RepoInfo
    {
        if (name.empty())
        {
            return solver::libsolv::RepoInfo(
                pool().add_repo(util::generate_random_alphanumeric_string(20)).second.raw()
            );
        }
        return solver::libsolv::RepoInfo(pool().add_repo(name).second.raw());
    }

    void MPool::add_repo_from_packages_impl_loop(
        const solver::libsolv::RepoInfo& repo,
        const specs::PackageInfo& pkg
    )
    {
        auto s_repo = solv::ObjRepoView(*repo.m_ptr);
        auto [id, solv] = s_repo.add_solvable();
        solver::libsolv::set_solvable(pool(), solv, pkg);
    }

    void MPool::add_repo_from_packages_impl_post(
        const solver::libsolv::RepoInfo& repo,
        solver::libsolv::PipAsPythonDependency add
    )
    {
        auto s_repo = solv::ObjRepoView(*repo.m_ptr);
        if (add == solver::libsolv::PipAsPythonDependency::Yes)
        {
            solver::libsolv::add_pip_as_python_dependency(pool(), s_repo);
        }
        s_repo.internalize();
    }

    auto MPool::native_serialize_repo(
        const solver::libsolv::RepoInfo& repo,
        const fs::u8path& path,
        const solver::libsolv::RepodataOrigin& metadata
    ) -> expected_t<solver::libsolv::RepoInfo>
    {
        assert(repo.m_ptr != nullptr);
        return solver::libsolv::write_solv(solv::ObjRepoView(*repo.m_ptr), path, metadata)
            .transform([](solv::ObjRepoView repo) { return solver::libsolv::RepoInfo(repo.raw()); });
    }

    void MPool::remove_repo(::Id repo_id, bool reuse_ids)
    {
        pool().remove_repo(repo_id, reuse_ids);
    }

    auto MPool::installed_repo() const -> std::optional<solver::libsolv::RepoInfo>
    {
        if (auto repo = pool().installed_repo())
        {
            // Safe because the Repo does not modify its internals
            return solver::libsolv::RepoInfo(const_cast<::Repo*>(repo->raw()));
        }
        return {};
    }

    void MPool::set_installed_repo(solver::libsolv::RepoInfo repo)
    {
        pool().set_installed_repo(repo.id());
    }

    void
    MPool::set_repo_priority(solver::libsolv::RepoInfo repo, solver::libsolv::Priorities priorities)
    {
        // NOTE: The Pool is not involved directly in this operations, but since it is needed
        // in so many repo operations, this setter was put here to keep the Repo class
        // immutable.
        static_assert(std::is_same_v<decltype(repo.m_ptr->priority), solver::libsolv::Priorities::value_type>);
        repo.m_ptr->priority = priorities.priority;
        repo.m_ptr->subpriority = priorities.subpriority;
    }

    auto MPool::package_id_to_package_info(PackageId id) const -> specs::PackageInfo
    {
        static_assert(std::is_same_v<std::underlying_type_t<PackageId>, solv::SolvableId>);
        // Safe because the ID is coming from libsolv
        return id2pkginfo(static_cast<solv::SolvableId>(id)).value();
    }

    auto MPool::packages_in_repo(solver::libsolv::RepoInfo repo) const -> std::vector<PackageId>
    {
        // TODO maybe we could use a span here depending on libsolv layout
        auto solv_repo = solv::ObjRepoViewConst(*repo.m_ptr);
        auto out = std::vector<PackageId>();
        out.reserve(solv_repo.solvable_count());
        solv_repo.for_each_solvable_id([&](auto id) { out.push_back(static_cast<PackageId>(id)); });
        return out;
    }

    auto MPool::packages_matching_ids(const specs::MatchSpec& ms) -> std::vector<PackageId>
    {
        static_assert(std::is_same_v<std::underlying_type_t<PackageId>, solv::SolvableId>);

        pool().ensure_whatprovides();
        const auto ms_id = matchspec2id(ms);
        auto solvables = pool().select_solvables({ SOLVER_SOLVABLE_PROVIDES, ms_id });
        auto out = std::vector<PackageId>(solvables.size());
        std::transform(
            solvables.begin(),
            solvables.end(),
            out.begin(),
            [](auto id) { return static_cast<PackageId>(id); }
        );
        return out;
    }

    auto MPool::packages_depending_on_ids(const specs::MatchSpec& ms) -> std::vector<PackageId>
    {
        static_assert(std::is_same_v<std::underlying_type_t<PackageId>, solv::SolvableId>);

        pool().ensure_whatprovides();
        const auto ms_id = matchspec2id(ms);
        auto solvables = pool().what_matches_dep(SOLVABLE_REQUIRES, ms_id);
        auto out = std::vector<PackageId>(solvables.size());
        std::transform(
            solvables.begin(),
            solvables.end(),
            out.begin(),
            [](auto id) { return static_cast<PackageId>(id); }
        );
        return out;
    }

    // TODO machinery functions in separate files

    void add_spdlog_logger_to_pool(MPool& pool)
    {
        pool.set_logger(
            [logger = spdlog::get("libsolv")](solver::libsolv::LogLevel level, std::string_view msg)
            {
                switch (level)
                {
                    case (solver::libsolv::LogLevel::Fatal):
                        logger->critical(msg);
                    case (solver::libsolv::LogLevel::Error):
                        logger->error(msg);
                    case (solver::libsolv::LogLevel::Warning):
                        logger->warn(msg);
                    case (solver::libsolv::LogLevel::Debug):
                        logger->debug(msg);
                }
            }
        );
    }

    auto load_subdir_in_pool(const Context& ctx, MPool& pool, const SubdirData& subdir)
        -> expected_t<solver::libsolv::RepoInfo>
    {
        const auto expected_cache_origin = solver::libsolv::RepodataOrigin{
            /* .url= */ util::rsplit(subdir.metadata().url(), "/", 1).front(),
            /* .etag= */ subdir.metadata().etag(),
            /* .mod= */ subdir.metadata().last_modified(),
        };

        const auto add_pip = static_cast<solver::libsolv::PipAsPythonDependency>(
            ctx.add_pip_as_python_dependency
        );
        const auto json_parser = ctx.experimental_repodata_parsing
                                     ? solver::libsolv::RepodataParser::Mamba
                                     : solver::libsolv::RepodataParser::Libsolv;

        // Solv files are too slow on Windows.
        if (!util::on_win)
        {
            auto maybe_repo = subdir.valid_solv_cache().and_then(
                [&](fs::u8path&& solv_file) {
                    return pool.add_repo_from_native_serialization(
                        solv_file,
                        expected_cache_origin,
                        add_pip
                    );
                }
            );
            if (maybe_repo)
            {
                return maybe_repo;
            }
        }

        return subdir.valid_json_cache()
            .and_then(
                [&](fs::u8path&& repodata_json)
                {
                    LOG_INFO << "Trying to load repo from json file " << repodata_json;
                    return pool.add_repo_from_repodata_json(
                        repodata_json,
                        util::rsplit(subdir.metadata().url(), "/", 1).front(),
                        add_pip,
                        static_cast<solver::libsolv::UseOnlyTarBz2>(ctx.use_only_tar_bz2),
                        json_parser
                    );
                }
            )
            .transform(
                [&](solver::libsolv::RepoInfo&& repo) -> solver::libsolv::RepoInfo
                {
                    if (!util::on_win)
                    {
                        pool.native_serialize_repo(repo, subdir.writable_solv_cache(), expected_cache_origin)
                            .or_else(
                                [&](const auto& err)
                                {
                                    LOG_WARNING << R"(Fail to write native serialization to file ")"
                                                << subdir.writable_solv_cache() << R"(" for repo ")"
                                                << subdir.name() << ": " << err.what();
                                    ;
                                }
                            );
                    }
                    return std::move(repo);
                }
            );
    }

    auto load_installed_packages_in_pool(const Context& ctx, MPool& pool, const PrefixData& prefix)
        -> solver::libsolv::RepoInfo
    {
        // TODO(C++20): We could do a PrefixData range that returns packages without storing thems.
        auto pkgs = prefix.sorted_records();
        // TODO(C++20): We only need a range that concatenate both
        for (auto&& pkg : get_virtual_packages(ctx))
        {
            pkgs.push_back(std::move(pkg));
        }

        // Not adding Pip dependency since it might needlessly make the installed/active environment
        // broken if pip is not already installed (debatable).
        auto repo = pool.add_repo_from_packages(
            pkgs,
            "installed",
            solver::libsolv::PipAsPythonDependency::No
        );
        pool.set_installed_repo(repo);
        return repo;
    }
}
