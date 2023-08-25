// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/repoquery.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    void repoquery(
        Configuration& config,
        QueryType type,
        QueryResultFormat format,
        bool use_local,
        const std::string& query
    )
    {
        auto& ctx = Context::instance();

        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX);
        config.load();

        ChannelContext channel_context;
        MPool pool{ channel_context };

        // bool installed = (type == QueryType::kDepends) || (type == QueryType::kWhoneeds);
        MultiPackageCache package_caches(ctx.pkgs_dirs);
        if (use_local)
        {
            if (format != QueryResultFormat::kJSON)
            {
                Console::stream() << "Using local repodata..." << std::endl;
            }
            auto exp_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!exp_prefix_data)
            {
                // TODO: propagate tl::expected mechanism
                throw std::runtime_error(exp_prefix_data.error().what());
            }
            PrefixData& prefix_data = exp_prefix_data.value();
            MRepo(pool, prefix_data);
            if (format != QueryResultFormat::kJSON)
            {
                Console::stream() << "Loaded current active prefix: "
                                  << ctx.prefix_params.target_prefix << std::endl;
            }
        }
        else
        {
            if (format != QueryResultFormat::kJSON)
            {
                Console::stream() << "Getting repodata from channels..." << std::endl;
            }
            auto exp_load = load_channels(pool, package_caches, 0);
            if (!exp_load)
            {
                throw std::runtime_error(exp_load.error().what());
            }
        }

        Query q(pool);
        if (type == QueryType::kSEARCH)
        {
            if (ctx.output_params.json)
            {
                std::cout << q.find(query).groupby("name").json(pool.channel_context()).dump(4);
            }
            else
            {
                std::cout << "\n" << std::endl;
                auto res = q.find(query);
                switch (format)
                {
                    case QueryResultFormat::kJSON:
                        std::cout << res.json(pool.channel_context()).dump(4);
                        break;
                    case QueryResultFormat::kPRETTY:
                        res.pretty(std::cout);
                        break;
                    default:
                        res.groupby("name").table(std::cout);
                }
                if (res.empty())
                {
                    std::cout << "Channels may not be configured. Try giving a channel with '-c,--channel' option, or use `--use-local=1` to search for installed packages."
                              << std::endl;
                }
            }
        }
        else if (type == QueryType::kDEPENDS)
        {
            auto res = q.depends(
                query,
                format == QueryResultFormat::kTREE || format == QueryResultFormat::kRECURSIVETABLE
            );
            switch (format)
            {
                case QueryResultFormat::kTREE:
                case QueryResultFormat::kPRETTY:
                    res.tree(std::cout);
                    break;
                case QueryResultFormat::kJSON:
                    std::cout << res.json(pool.channel_context()).dump(4);
                    break;
                case QueryResultFormat::kTABLE:
                case QueryResultFormat::kRECURSIVETABLE:
                    res.sort("name").table(std::cout);
            }
            if (res.empty() && format != QueryResultFormat::kJSON)
            {
                std::cout << query
                          << " may not be installed. Try giving a channel with '-c,--channel' option for remote repoquery"
                          << std::endl;
            }
        }
        else if (type == QueryType::kWHONEEDS)
        {
            auto res = q.whoneeds(
                query,
                format == QueryResultFormat::kTREE || format == QueryResultFormat::kRECURSIVETABLE
            );
            switch (format)
            {
                case QueryResultFormat::kTREE:
                case QueryResultFormat::kPRETTY:
                    res.tree(std::cout);
                    break;
                case QueryResultFormat::kJSON:
                    std::cout << res.json(pool.channel_context()).dump(4);
                    break;
                case QueryResultFormat::kTABLE:
                case QueryResultFormat::kRECURSIVETABLE:
                    res.sort("name").table(
                        std::cout,
                        { "Name", "Version", "Build", "", "", util::concat("Depends:", query), "Channel", "Subdir" }
                    );
            }
            if (res.empty() && format != QueryResultFormat::kJSON)
            {
                std::cout << query
                          << " may not be installed. Try giving a channel with '-c,--channel' option for remote repoquery"
                          << std::endl;
            }
        }
    }
}
