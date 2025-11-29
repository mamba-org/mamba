// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <string>
#include <vector>

#include "mamba/api/configuration.hpp"
#include "mamba/api/repoquery.hpp"

#include "common_options.hpp"

namespace
{
    struct RepoqueryOptions
    {
        bool show_as_tree = false;
        bool recursive = false;
        bool pretty_print = false;
        std::vector<std::string> specs = {};
    };

    void
    init_repoquery_common_options(CLI::App* subcmd, mamba::Configuration& config, RepoqueryOptions& options)
    {
        subcmd->add_flag("-t,--tree", options.show_as_tree, "Show result as a tree");

        subcmd->add_flag(
            "--recursive",
            options.recursive,
            "Show dependencies recursively, i.e. transitive dependencies (only for `depends`)"
        );

        subcmd->add_flag("--pretty", options.pretty_print, "Pretty print result (only for search)");

        subcmd->add_option("specs", options.specs, "Specs to search")->required();

        auto& platform = config.at("platform");
        subcmd
            ->add_option("--platform", platform.get_cli_config<std::string>(), platform.description())
            ->option_text("PLATFORM");
    }

    template <typename Iter>
    auto specs_has_wildcard(Iter first, Iter last) -> bool
    {
        auto has_wildcard = [](std::string_view spec) -> bool
        { return spec.find('*') != std::string_view::npos; };
        return std::any_of(first, last, has_wildcard);
    };

    auto
    compute_format(mamba::QueryType query, mamba::Configuration& config, RepoqueryOptions& options)
    {
        auto format = mamba::QueryResultFormat::Table;
        if (query == mamba::QueryType::Depends && options.recursive)
        {
            format = mamba::QueryResultFormat::RecursiveTable;
        }

        if (query == mamba::QueryType::Depends && options.show_as_tree)
        {
            format = mamba::QueryResultFormat::Tree;
        }
        // Best guess to detect wildcard search; if there's no wildcard search, we want to
        // show the pretty single package view.
        if (query == mamba::QueryType::Search
            && (options.pretty_print
                || !specs_has_wildcard(options.specs.cbegin(), options.specs.cend())))
        {
            format = mamba::QueryResultFormat::Pretty;
        }

        if (config.at("json").compute().value<bool>())
        {
            format = mamba::QueryResultFormat::Json;
        }
        return format;
    }

    template <mamba::QueryType query>
    void set_repoquery_subcommand(CLI::App* subcmd, mamba::Configuration& config)
    {
        init_general_options(subcmd, config);
        init_prefix_options(subcmd, config);
        init_network_options(subcmd, config);
        init_channel_parser(subcmd, config);

        static auto options = RepoqueryOptions{};
        init_repoquery_common_options(subcmd, config, options);

        static bool remote = false;
        subcmd->add_flag("--remote", remote, "Use remote repositories");

        subcmd->callback(
            [&]()
            {
                // Set remote when a channel is passed
                const bool channel_passed = config.at("channels").cli_configured();
                const auto format = compute_format(query, config, options);
                const bool success = repoquery(
                    config,
                    query,
                    format,
                    !(remote | channel_passed),
                    options.specs
                );
                if (!success && (format != mamba::QueryResultFormat::Json))
                {
                    if (!remote)
                    {
                        std::cout << "Try looking remotely with '--remote'.\n";
                    }
                    if (remote && !channel_passed)
                    {
                        std::cout << "Try looking in a different channel with '-c, --channel'.\n";
                    }
                }
            }
        );
    }

    template <>
    void
    set_repoquery_subcommand<mamba::QueryType::Search>(CLI::App* subcmd, mamba::Configuration& config)
    {
        static constexpr auto query = mamba::QueryType::Search;

        init_general_options(subcmd, config);
        init_prefix_options(subcmd, config);
        init_network_options(subcmd, config);
        init_channel_parser(subcmd, config);

        static auto options = RepoqueryOptions{};
        init_repoquery_common_options(subcmd, config, options);

        static bool use_local = false;
        subcmd->add_flag("--local", use_local, "Use installed prefix instead of remote repositories");

        subcmd->callback(
            [&]()
            {
                const bool channel_passed = config.at("channels").cli_configured();
                const auto format = compute_format(query, config, options);
                const bool success = repoquery(config, query, format, use_local, options.specs);
                if (!success && (format != mamba::QueryResultFormat::Json))
                {
                    if (!use_local && !channel_passed)
                    {
                        std::cout << "Try looking in a different channel with '-c, --channel'.\n";
                    }
                }
            }
        );
    }
}

void
set_repoquery_search_command(CLI::App* subcmd, mamba::Configuration& config)
{
    set_repoquery_subcommand<mamba::QueryType::Search>(subcmd, config);
}

void
set_repoquery_whoneeds_command(CLI::App* subcmd, mamba::Configuration& config)
{
    set_repoquery_subcommand<mamba::QueryType::WhoNeeds>(subcmd, config);
}

void
set_repoquery_depends_command(CLI::App* subcmd, mamba::Configuration& config)
{
    set_repoquery_subcommand<mamba::QueryType::Depends>(subcmd, config);
}

void
set_repoquery_command(CLI::App* subcmd, mamba::Configuration& config)
{
    {
        auto* search_subsubcmd = subcmd->add_subcommand(
            "search",
            "Search for packages matching a given query"
        );
        set_repoquery_search_command(search_subsubcmd, config);
    }
    {
        auto* whoneeds_subsubcmd = subcmd->add_subcommand(
            "whoneeds",
            "List packages that needs the given query as a dependency"
        );
        set_repoquery_whoneeds_command(whoneeds_subsubcmd, config);
    }
    {
        auto* depends_subsubcmd = subcmd->add_subcommand("depends", "List dependencies of a given query");
        set_repoquery_depends_command(depends_subsubcmd, config);
    }
}
