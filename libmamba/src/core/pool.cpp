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
#include "mamba/core/pool.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/string.hpp"

#include "solver/libsolv/helpers.hpp"

namespace mamba
{
    void add_spdlog_logger_to_database(solver::libsolv::Database& db)
    {
        db.set_logger(
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

    auto
    load_subdir_in_database(const Context& ctx, solver::libsolv::Database& db, const SubdirData& subdir)
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
                    return db.add_repo_from_native_serialization(
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
                    return db.add_repo_from_repodata_json(
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
                        db.native_serialize_repo(repo, subdir.writable_solv_cache(), expected_cache_origin)
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

    auto load_installed_packages_in_database(
        const Context& ctx,
        solver::libsolv::Database& db,
        const PrefixData& prefix
    ) -> solver::libsolv::RepoInfo
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
        auto repo = db.add_repo_from_packages(
            pkgs,
            "installed",
            solver::libsolv::PipAsPythonDependency::No
        );
        db.set_installed_repo(repo);
        return repo;
    }
}
