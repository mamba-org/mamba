// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/repoquery.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace
    {
        auto
        repoquery_init(Context& ctx, Configuration& config, QueryResultFormat format, bool use_local)
        {
            config.at("use_target_prefix_fallback").set_value(true);
            config.at("use_default_prefix_fallback").set_value(true);
            config.at("use_root_prefix_fallback").set_value(true);
            config.at("target_prefix_checks")
                .set_value(
                    MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX | MAMBA_ALLOW_NOT_ENV_PREFIX
                );
            config.load();

            auto channel_context = ChannelContext::make_conda_compatible(ctx);
            solver::libsolv::Database db{
                channel_context.params(),
                {
                    ctx.experimental_matchspec_parsing ? solver::libsolv::MatchSpecParser::Mamba
                                                       : solver::libsolv::MatchSpecParser::Libsolv,
                },
            };
            add_logger_to_database(db);

            // bool installed = (type == QueryType::kDepends) || (type == QueryType::kWhoneeds);
            MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);
            if (use_local)
            {
                if (format != QueryResultFormat::Json)
                {
                    Console::stream() << "Using local repodata..." << std::endl;
                }
                auto exp_prefix_data = PrefixData::create(
                    ctx.prefix_params.target_prefix,
                    channel_context
                );
                if (!exp_prefix_data)
                {
                    // TODO: propagate tl::expected mechanism
                    throw std::runtime_error(exp_prefix_data.error().what());
                }
                PrefixData& prefix_data = exp_prefix_data.value();

                load_installed_packages_in_database(ctx, db, prefix_data);

                if (format != QueryResultFormat::Json)
                {
                    Console::stream()
                        << "Loaded current active prefix: " << ctx.prefix_params.target_prefix
                        << std::endl;
                }
            }
            else
            {
                if (format != QueryResultFormat::Json)
                {
                    Console::stream() << "Getting repodata from channels..." << std::endl;
                }
                // No root packages for query operations (will use traditional repodata)
                auto exp_load = load_channels(ctx, channel_context, db, package_caches, {});
                if (!exp_load)
                {
                    throw std::runtime_error(exp_load.error().what());
                }
            }
            return db;
        }
    }

    auto make_repoquery(
        solver::libsolv::Database& database,
        QueryType type,
        QueryResultFormat format,
        const std::vector<std::string>& queries,
        bool show_all_builds,
        const Context::GraphicsParams& graphics_params,
        std::ostream& out
    ) -> bool
    {
        if (type == QueryType::Search)
        {
            auto res = Query::find(database, queries);
            switch (format)
            {
                case QueryResultFormat::Json:
                    out << res.groupby("name").json().dump(4);
                    break;
                case QueryResultFormat::Pretty:
                    res.pretty(out, show_all_builds);
                    break;
                default:
                    res.groupby("name").table(out);
            }
            return !res.empty();
        }
        else if (type == QueryType::Depends)
        {
            if (queries.size() != 1)
            {
                throw std::invalid_argument("Only one query supported for 'depends'.");
            }
            auto res = Query::depends(
                database,
                queries.front(),
                /* tree= */ format == QueryResultFormat::Tree
                    || format == QueryResultFormat::RecursiveTable
            );
            switch (format)
            {
                case QueryResultFormat::Tree:
                case QueryResultFormat::Pretty:
                    res.tree(out, graphics_params);
                    break;
                case QueryResultFormat::Json:
                    out << res.json().dump(4);
                    break;
                case QueryResultFormat::Table:
                case QueryResultFormat::RecursiveTable:
                    res.sort("name").table(out);
            }
            return !res.empty();
        }
        else if (type == QueryType::WhoNeeds)
        {
            if (queries.size() != 1)
            {
                throw std::invalid_argument("Only one query supported for 'whoneeds'.");
            }
            auto res = Query::whoneeds(
                database,
                queries.front(),
                /* tree= */ format == QueryResultFormat::Tree
                    || format == QueryResultFormat::RecursiveTable
            );
            switch (format)
            {
                case QueryResultFormat::Tree:
                case QueryResultFormat::Pretty:
                    res.tree(out, graphics_params);
                    break;
                case QueryResultFormat::Json:
                    out << res.json().dump(4);
                    break;
                case QueryResultFormat::Table:
                case QueryResultFormat::RecursiveTable:
                    res.sort("name").table(
                        out,
                        { "Name",
                          "Version",
                          "Build",
                          printers::alignmentMarker(printers::alignment::left),
                          printers::alignmentMarker(printers::alignment::right),
                          util::concat("Depends:", queries.front()),
                          "Channel",
                          "Subdir" }
                    );
            }
            return !res.empty();
        }
        throw std::invalid_argument("Invalid QueryType");
    }

    bool repoquery(
        Configuration& config,
        QueryType type,
        QueryResultFormat format,
        bool use_local,
        const std::vector<std::string>& queries
    )
    {
        auto& ctx = config.context();
        auto db = repoquery_init(ctx, config, format, use_local);
        return make_repoquery(
            db,
            type,
            format,
            queries,
            ctx.output_params.verbosity > 0,
            ctx.graphics_params,
            std::cout
        );
    }

}
