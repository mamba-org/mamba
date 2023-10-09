// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/repoquery.hpp"

#include "common_options.hpp"

void
set_common_search(CLI::App* subcom, mamba::Configuration& config, bool is_repoquery)
{
    init_general_options(subcom, config);
    init_prefix_options(subcom, config);
    init_network_options(subcom, config);
    init_channel_parser(subcom, config);

    static std::string query_type;
    if (is_repoquery)
    {
        subcom
            ->add_option("query_type", query_type, "The type of query (search, depends or whoneeds)")
            ->check(CLI::IsMember(std::vector<std::string>({ "search", "depends", "whoneeds" })))
            ->required();
    }
    else
    {
        // `search` and `repoquery search` are equivalent
        query_type = "search";
    }

    static bool show_as_tree = false;
    subcom->add_flag("-t,--tree", show_as_tree, "Show result as a tree");

    static bool recursive = false;
    subcom->add_flag(
        "--recursive",
        recursive,
        "Show dependencies recursively, i.e. transitive dependencies (only for `depends`)"
    );

    static bool pretty_print = false;
    subcom->add_flag("--pretty", pretty_print, "Pretty print result (only for search)");

    static std::vector<std::string> specs;
    subcom->add_option("specs", specs, "Specs to search")->required();

    static int local = -1;
    subcom->add_option(
        "--use-local",
        local,
        "Use installed data (--use-local=1, default for `depends` and `whoneeds`) or remote repositories (--use-local=0, default for `search`).\nIf the `-c,--channel` option is set, it has the priority and --use-local is set to 0"
    );

    auto& platform = config.at("platform");
    subcom->add_option("--platform", platform.get_cli_config<std::string>(), platform.description());

    auto specs_has_wildcard = [](auto first, auto last) -> bool
    {
        auto has_wildcard = [](std::string_view spec) -> bool
        { return spec.find('*') != std::string_view::npos; };
        return std::any_of(first, last, has_wildcard);
    };

    subcom->callback(
        [&]
        {
            using namespace mamba;

            auto qtype = QueryType_from_name(query_type);
            QueryResultFormat format = QueryResultFormat::Table;
            bool use_local = true;
            switch (qtype)
            {
                case QueryType::Search:
                    format = QueryResultFormat::Table;
                    use_local = local > 0;  // use remote repodata by default for `search`
                    break;
                case QueryType::Depends:
                case QueryType::WhoNeeds:
                    format = QueryResultFormat::Table;
                    use_local = (local == -1) || (local > 0);
                    break;
            }
            if (qtype == QueryType::Depends && recursive)
            {
                format = QueryResultFormat::RecursiveTable;
            }

            if (qtype == QueryType::Depends && show_as_tree)
            {
                format = QueryResultFormat::Tree;
            }
            // Best guess to detect wildcard search; if there's no wildcard search, we want to show
            // the pretty single package view.
            if (qtype == QueryType::Search
                && (pretty_print || specs_has_wildcard(specs.cbegin(), specs.cend())))
            {
                format = QueryResultFormat::Pretty;
            }

            if (config.at("json").compute().value<bool>())
            {
                format = QueryResultFormat::Json;
            }

            auto& channels = config.at("channels").compute().value<std::vector<std::string>>();
            use_local = use_local && channels.empty();

            repoquery(config, qtype, format, use_local, specs);
        }
    );
}

void
set_search_command(CLI::App* subcom, mamba::Configuration& config)
{
    set_common_search(subcom, config, false);
}

void
set_repoquery_command(CLI::App* subcom, mamba::Configuration& config)
{
    set_common_search(subcom, config, true);
}
