// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string_view>

#include <fmt/format.h>
#include <solv/evr.h>
#include <solv/selection.h>
#include <solv/solver.h>
#include <spdlog/spdlog.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/string.hpp"

#include "solver/libsolv/helpers.hpp"

namespace mamba
{
    void add_spdlog_logger_to_database(solver::libsolv::Database& database)
    {
        database.set_logger(
            [logger = spdlog::get("libsolv")](solver::libsolv::LogLevel level, std::string_view msg)
            {
                switch (level)
                {
                    case (solver::libsolv::LogLevel::Fatal):
                        logger->critical(msg);
                        break;
                    case (solver::libsolv::LogLevel::Error):
                        logger->error(msg);
                        break;
                    case (solver::libsolv::LogLevel::Warning):
                        logger->warn(msg);
                        break;
                    case (solver::libsolv::LogLevel::Debug):
                        logger->debug(msg);
                        break;
                }
            }
        );
    }

    auto load_subdir_in_database(
        const Context& ctx,
        solver::libsolv::Database& database,
        const SubdirIndexLoader& subdir
    ) -> expected_t<solver::libsolv::RepoInfo>
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
            auto maybe_repo = subdir.valid_libsolv_cache_path().and_then(
                [&](fs::u8path&& solv_file)
                {
                    return database.add_repo_from_native_serialization(
                        solv_file,
                        expected_cache_origin,
                        subdir.channel_id(),
                        add_pip
                    );
                }
            );
            if (maybe_repo)
            {
                return maybe_repo;
            }
        }

        return subdir.valid_json_cache_path()
            .and_then(
                [&](fs::u8path&& repodata_json)
                {
                    using PackageTypes = solver::libsolv::PackageTypes;

                    LOG_INFO << "Trying to load repo from json file " << repodata_json;
                    return database.add_repo_from_repodata_json(
                        repodata_json,
                        util::rsplit(subdir.metadata().url(), "/", 1).front(),
                        subdir.channel_id(),
                        add_pip,
                        ctx.use_only_tar_bz2 ? PackageTypes::TarBz2Only
                                             : PackageTypes::CondaOrElseTarBz2,
                        static_cast<solver::libsolv::VerifyPackages>(ctx.validation_params.verify_artifacts
                        ),
                        json_parser
                    );
                }
            )
            .transform(
                [&](solver::libsolv::RepoInfo&& repo) -> solver::libsolv::RepoInfo
                {
                    if (!util::on_win)
                    {
                        database
                            .native_serialize_repo(
                                repo,
                                subdir.writable_libsolv_cache_path(),
                                expected_cache_origin
                            )
                            .or_else(
                                [&](const auto& err)
                                {
                                    LOG_WARNING << R"(Fail to write native serialization to file ")"
                                                << subdir.writable_libsolv_cache_path()
                                                << R"(" for repo ")" << subdir.name() << ": "
                                                << err.what();
                                    ;
                                }
                            );
                    }
                    return std::move(repo);
                }
            );
    }

    auto load_installed_packages_in_database(
        const Context& ctx,
        solver::libsolv::Database& database,
        const PrefixData& prefix
    ) -> solver::libsolv::RepoInfo
    {
        // TODO(C++20): We could do a PrefixData range that returns packages without storing them.
        auto pkgs = prefix.sorted_records();
        // TODO(C++20): We only need a range that concatenate both
        for (auto&& pkg : get_virtual_packages(ctx.platform))
        {
            pkgs.push_back(std::move(pkg));
        }

        // Not adding Pip dependency since it might needlessly make the installed/active environment
        // broken if pip is not already installed (debatable).
        auto repo = database.add_repo_from_packages(
            pkgs,
            "installed",
            solver::libsolv::PipAsPythonDependency::No
        );
        database.set_installed_repo(repo);
        return repo;
    }
}
