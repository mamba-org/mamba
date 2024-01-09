// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include <solv/solver.h>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/repoquery.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace
    {
        auto
        repoquery_init(Context& ctx, Configuration& config, QueryResultFormat format, bool use_local)
        {
            config.at("use_target_prefix_fallback").set_value(true);
            config.at("target_prefix_checks")
                .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX);
            config.load();

            auto channel_context = ChannelContext::make_conda_compatible(ctx);
            MPool pool{ ctx, channel_context };

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

                const auto repo = MRepo(
                    pool,
                    "installed",
                    prefix_data.sorted_records(),
                    MRepo::PipAsPythonDependency::Yes
                );
                pool.set_installed_repo(repo);

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
                auto exp_load = load_channels(ctx, pool, package_caches, 0);
                if (!exp_load)
                {
                    throw std::runtime_error(exp_load.error().what());
                }
            }
            return pool;
        }
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
        auto pool = repoquery_init(ctx, config, format, use_local);
        Query q(pool);


        if (type == QueryType::Search)
        {
            auto res = q.find(queries);
            switch (format)
            {
                case QueryResultFormat::Json:
                    std::cout << res.groupby("name").json().dump(4);
                    break;
                case QueryResultFormat::Pretty:
                    res.pretty(std::cout, ctx.output_params);
                    break;
                default:
                    res.groupby("name").table(std::cout);
            }
            return !res.empty();
        }
        else if (type == QueryType::Depends)
        {
            if (queries.size() != 1)
            {
                throw std::invalid_argument("Only one query supported for 'depends'.");
            }
            auto res = q.depends(
                queries.front(),
                format == QueryResultFormat::Tree || format == QueryResultFormat::RecursiveTable
            );
            switch (format)
            {
                case QueryResultFormat::Tree:
                case QueryResultFormat::Pretty:
                    res.tree(std::cout, config.context().graphics_params);
                    break;
                case QueryResultFormat::Json:
                    std::cout << res.json().dump(4);
                    break;
                case QueryResultFormat::Table:
                case QueryResultFormat::RecursiveTable:
                    res.sort("name").table(std::cout);
            }
            return !res.empty();
        }
        else if (type == QueryType::WhoNeeds)
        {
            if (queries.size() != 1)
            {
                throw std::invalid_argument("Only one query supported for 'whoneeds'.");
            }
            auto res = q.whoneeds(
                queries.front(),
                format == QueryResultFormat::Tree || format == QueryResultFormat::RecursiveTable
            );
            switch (format)
            {
                case QueryResultFormat::Tree:
                case QueryResultFormat::Pretty:
                    res.tree(std::cout, config.context().graphics_params);
                    break;
                case QueryResultFormat::Json:
                    std::cout << res.json().dump(4);
                    break;
                case QueryResultFormat::Table:
                case QueryResultFormat::RecursiveTable:
                    res.sort("name").table(
                        std::cout,
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
    }
}
