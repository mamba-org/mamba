// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string_view>
#include <variant>

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
        std::variant<
            std::reference_wrapper<solver::libsolv::Database>,
            std::reference_wrapper<solver::resolvo::Database>> database,
        const SubdirIndexLoader& subdir
    ) -> expected_t<void>
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
                    return std::visit(
                        [&](auto& db) -> expected_t<void>
                        {
                            using DB = std::decay_t<decltype(db)>;
                            if constexpr (std::is_same_v<DB, std::reference_wrapper<solver::libsolv::Database>>)
                            {
                                db.get().add_repo_from_native_serialization(
                                    solv_file,
                                    expected_cache_origin,
                                    subdir.channel_id(),
                                    add_pip
                                );
                                return {};
                            }
                            else
                            {
                                return make_unexpected(
                                    "Native serialization not supported for resolvo::Database",
                                    mamba_error_code::unknown
                                );
                            }
                        },
                        database
                    );
                }
            );
            if (maybe_repo)
            {
                return {};
            }
        }

        return subdir.valid_json_cache_path()
            .and_then(
                [&](fs::u8path&& repodata_json)
                {
                    using PackageTypes = solver::libsolv::PackageTypes;
                    LOG_INFO << "Trying to load repo from json file " << repodata_json;
                    return std::visit(
                        [&](auto& db) -> expected_t<void>
                        {
                            using DB = std::decay_t<decltype(db)>;
                            if constexpr (std::is_same_v<DB, std::reference_wrapper<solver::libsolv::Database>>)
                            {
                                db.get().add_repo_from_repodata_json(
                                    repodata_json,
                                    util::rsplit(subdir.metadata().url(), "/", 1).front(),
                                    subdir.channel_id(),
                                    add_pip,
                                    ctx.use_only_tar_bz2 ? PackageTypes::TarBz2Only
                                                         : PackageTypes::CondaOrElseTarBz2,
                                    static_cast<solver::libsolv::VerifyPackages>(
                                        ctx.validation_params.verify_artifacts
                                    ),
                                    json_parser
                                );
                                return {};
                            }
                            else
                            {
                                db.get().add_repo_from_repodata_json(
                                    repodata_json,
                                    util::rsplit(subdir.metadata().url(), "/", 1).front(),
                                    subdir.channel_id(),
                                    false
                                );
                                return {};
                            }
                        },
                        database
                    );
                }
            )
            .transform(
                [&](void) -> void
                {
                    // Serialization step removed: no RepoInfo available to serialize.
                }
            );
    }

    auto load_installed_packages_in_database(
        const Context& ctx,
        std::variant<
            std::reference_wrapper<solver::libsolv::Database>,
            std::reference_wrapper<solver::resolvo::Database>> database,
        const PrefixData& prefix
    ) -> expected_t<void>
    {
        auto pkgs = prefix.sorted_records();
        for (auto&& pkg : get_virtual_packages(ctx.platform))
        {
            pkgs.push_back(std::move(pkg));
        }

        if (auto* libsolv_db = std::get_if<std::reference_wrapper<solver::libsolv::Database>>(&database
            ))
        {
            auto repo = libsolv_db->get().add_repo_from_packages(
                pkgs,
                "installed",
                solver::libsolv::PipAsPythonDependency::No
            );
            libsolv_db->get().set_installed_repo(repo);
            return {};
        }
        else if (auto* resolvo_db = std::get_if<std::reference_wrapper<solver::resolvo::Database>>(
                     &database
                 ))
        {
            resolvo_db->get().add_repo_from_packages(pkgs, "installed");
            resolvo_db->get().set_installed_repo("installed");
            return {};
        }
        else
        {
            return make_unexpected("Unknown database type", mamba_error_code::unknown);
        }
    }
}
