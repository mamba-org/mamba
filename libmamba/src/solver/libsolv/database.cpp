// Copyright (c) 2023, QuantStack and Mamba Contributors
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

#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/random.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"

#include "solver/libsolv/helpers.hpp"
#include "solver/libsolv/matcher.hpp"

namespace mamba::solver::libsolv
{
    struct Database::DatabaseImpl
    {
        explicit DatabaseImpl(specs::ChannelResolveParams p_channel_params)
            : matcher(std::move(p_channel_params))
        {
        }

        solv::ObjPool pool = {};
        Matcher matcher;
    };

    Database::Database(specs::ChannelResolveParams channel_params)
        : m_data(std::make_unique<DatabaseImpl>(std::move(channel_params)))
    {
        pool().set_disttype(DISTTYPE_CONDA);
        // Ensure that debug logging never goes to stdout as to not interfere json output
        pool().raw()->debugmask |= SOLV_DEBUG_TO_STDERR;
        ::pool_setdebuglevel(pool().raw(), -1);  // Off
        pool().set_namespace_callback(
            [&data = (*m_data
             )](solv::ObjPoolView pool, solv::StringId first, solv::StringId second) -> solv::OffsetId
            {
                auto [dep, flags] = get_abused_namespace_callback_args(pool, first, second);
                return data.matcher.get_matching_packages(pool, dep, flags);
            }
        );
    }

    Database::~Database() = default;

    Database::Database(Database&&) = default;

    auto Database::operator=(Database&&) -> Database& = default;

    auto Database::pool() -> solv::ObjPool&
    {
        return m_data->pool;
    }

    auto Database::pool() const -> const solv::ObjPool&
    {
        return m_data->pool;
    }

    auto Database::Impl::get(Database& database) -> solv::ObjPool&
    {
        return database.pool();
    }

    auto Database::Impl::get(const Database& database) -> const solv::ObjPool&
    {
        return database.pool();
    }

    auto Database::channel_params() const -> const specs::ChannelResolveParams&
    {
        return m_data->matcher.channel_params();
    }

    namespace
    {
        auto libsolv_to_log_level(int type) -> LogLevel
        {
            if (type & SOLV_FATAL)
            {
                return LogLevel::Fatal;
            }
            if (type & SOLV_ERROR)
            {
                return LogLevel::Error;
            }
            if (type & SOLV_WARN)
            {
                return LogLevel::Warning;
            }
            return LogLevel::Debug;
        }
    }

    void Database::set_logger(logger_type callback)
    {
        // We must not use more than the penultimate level of verbosity of libsolv (which is 3) to
        // avoid the most verbose messages (of type SOLV_DEBUG_RULE_CREATION | SOLV_DEBUG_WATCHES),
        // which might spam the output and make mamba hang. See:
        // https://github.com/openSUSE/libsolv/blob/27aa6a72c7db73d78aa711ae412231768e77c9e0/src/pool.c#L1623-L1637
        // TODO: Make `level` configurable once the semantics and UX for verbosity are clarified.
        // Currently, we use the behavior of `1.x` whose default value for the verbosity level was
        // `0` in which case `::pool_setdebuglevel` was not called. See:
        // https://github.com/mamba-org/mamba/blob/4f269258b4237a342da3e9891045cdd51debb27c/libmamba/include/mamba/core/context.hpp#L88
        // See: https://github.com/mamba-org/mamba/blob/1.x/libmamba/src/core/pool.cpp#L72
        // Instead use something like:
        // const int level = Context().output_params.verbosity - 1;
        // ::pool_setdebuglevel(pool().raw(), level);
        pool().set_debug_callback(
            [logger = std::move(callback)](const solv::ObjPoolView&, int type, std::string_view msg) noexcept
            {
                try
                {
                    logger(libsolv_to_log_level(type), msg);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Developer error: error in libsolv logging function: \n"
                              << e.what();
                }
            }
        );
    }

    auto Database::add_repo_from_repodata_json(
        const fs::u8path& path,
        std::string_view url,
        const std::string& channel_id,
        PipAsPythonDependency add,
        PackageTypes package_types,
        VerifyPackages verify_packages,
        RepodataParser parser
    ) -> expected_t<RepoInfo>
    {
        const auto verify_artifacts = static_cast<bool>(verify_packages);

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
            if (parser == RepodataParser::Mamba)
            {
                return mamba_read_json(
                    pool(),
                    repo,
                    path,
                    std::string(url),
                    channel_id,
                    package_types,
                    verify_artifacts
                );
            }
            return libsolv_read_json(repo, path, package_types, verify_artifacts)
                .transform(
                    [&url, &channel_id](solv::ObjRepoView p_repo)
                    {
                        set_solvables_url(p_repo, std::string(url), channel_id);
                        return p_repo;
                    }
                );
        };

        return make_repo()
            .transform(
                [&](solv::ObjRepoView p_repo) -> RepoInfo
                {
                    if (add == PipAsPythonDependency::Yes)
                    {
                        add_pip_as_python_dependency(pool(), p_repo);
                    }
                    p_repo.internalize();
                    return RepoInfo{ p_repo.raw() };
                }
            )
            .or_else([&](const auto&) { pool().remove_repo(repo.id(), /* reuse_ids= */ true); });
    }

    auto Database::add_repo_from_native_serialization(
        const fs::u8path& path,
        const RepodataOrigin& expected,
        const std::string& channel_id,
        PipAsPythonDependency add
    ) -> expected_t<RepoInfo>
    {
        auto repo = pool().add_repo(expected.url).second;

        return read_solv(pool(), repo, path, expected, static_cast<bool>(add))
            .transform(
                [&](solv::ObjRepoView p_repo) -> RepoInfo
                {
                    p_repo.set_url(expected.url);
                    set_solvables_url(p_repo, expected.url, channel_id);
                    if (add == PipAsPythonDependency::Yes)
                    {
                        add_pip_as_python_dependency(pool(), p_repo);
                    }
                    p_repo.internalize();
                    return RepoInfo(p_repo.raw());
                }
            )
            .or_else([&](const auto&) { pool().remove_repo(repo.id(), /* reuse_ids= */ true); });
    }

    auto Database::add_repo_from_packages_impl_pre(std::string_view name) -> RepoInfo
    {
        if (name.empty())
        {
            return RepoInfo(
                pool().add_repo(util::generate_random_alphanumeric_string(20)).second.raw()
            );
        }
        return RepoInfo(pool().add_repo(name).second.raw());
    }

    void
    Database::add_repo_from_packages_impl_loop(const RepoInfo& repo, const specs::PackageInfo& pkg)
    {
        auto s_repo = solv::ObjRepoView(*repo.m_ptr);
        auto [id, solv] = s_repo.add_solvable();
        set_solvable(pool(), solv, pkg);
    }

    void Database::add_repo_from_packages_impl_post(const RepoInfo& repo, PipAsPythonDependency add)
    {
        auto s_repo = solv::ObjRepoView(*repo.m_ptr);
        if (add == PipAsPythonDependency::Yes)
        {
            add_pip_as_python_dependency(pool(), s_repo);
        }
        s_repo.internalize();
    }

    auto Database::native_serialize_repo(
        const RepoInfo& repo,
        const fs::u8path& path,
        const RepodataOrigin& metadata
    ) -> expected_t<RepoInfo>
    {
        assert(repo.m_ptr != nullptr);
        return write_solv(solv::ObjRepoView(*repo.m_ptr), path, metadata)
            .transform([](solv::ObjRepoView solv_repo) { return RepoInfo(solv_repo.raw()); });
    }

    void Database::remove_repo(RepoInfo repo)
    {
        pool().remove_repo(repo.id(), /* reuse_ids= */ true);
    }

    auto Database::repo_count() const -> std::size_t
    {
        return pool().repo_count();
    }

    auto Database::package_count() const -> std::size_t
    {
        return pool().solvable_count();
    }

    auto Database::installed_repo() const -> std::optional<RepoInfo>
    {
        if (auto repo = pool().installed_repo())
        {
            // Safe because the Repo does not modify its internals
            return RepoInfo(const_cast<::Repo*>(repo->raw()));
        }
        return {};
    }

    void Database::set_installed_repo(RepoInfo repo)
    {
        pool().set_installed_repo(repo.id());
    }

    void Database::set_repo_priority(RepoInfo repo, Priorities priorities)
    {
        // NOTE: The Pool is not involved directly in this operations, but since it is needed
        // in so many repo operations, this setter was put here to keep the Repo class
        // immutable.
        static_assert(std::is_same_v<decltype(repo.m_ptr->priority), Priorities::value_type>);
        repo.m_ptr->priority = priorities.priority;
        repo.m_ptr->subpriority = priorities.subpriority;
    }

    auto Database::package_id_to_package_info(PackageId id) const -> specs::PackageInfo
    {
        static_assert(std::is_same_v<std::underlying_type_t<PackageId>, solv::SolvableId>);
        const auto solv = pool().get_solvable(static_cast<solv::SolvableId>(id));
        assert(solv.has_value());  // Safe because the ID is coming from libsolv
        return { make_package_info(pool(), solv.value()) };
    }

    auto Database::packages_in_repo(RepoInfo repo) const -> std::vector<PackageId>
    {
        // TODO maybe we could use a span here depending on libsolv layout
        auto solv_repo = solv::ObjRepoViewConst(*repo.m_ptr);
        auto out = std::vector<PackageId>();
        out.reserve(solv_repo.solvable_count());
        solv_repo.for_each_solvable_id([&](auto id) { out.push_back(static_cast<PackageId>(id)); });
        return out;
    }

    namespace
    {
        auto pool_add_matchspec_throwing(solv::ObjPool& pool, const specs::MatchSpec& ms)
            -> solv::DependencyId
        {
            return pool_add_matchspec(pool, ms, MatchSpecParser::Mixed)
                .or_else([](mamba_error&& error) { throw std::move(error); })
                .value_or(0);
        }
    }

    auto Database::packages_matching_ids(const specs::MatchSpec& ms) -> std::vector<PackageId>
    {
        static_assert(std::is_same_v<std::underlying_type_t<PackageId>, solv::SolvableId>);

        pool().ensure_whatprovides();
        const auto ms_id = pool_add_matchspec_throwing(pool(), ms);
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

    auto Database::packages_depending_on_ids(const specs::MatchSpec& ms) -> std::vector<PackageId>
    {
        static_assert(std::is_same_v<std::underlying_type_t<PackageId>, solv::SolvableId>);

        pool().ensure_whatprovides();
        const auto ms_id = pool_add_matchspec_throwing(pool(), ms);
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
}
