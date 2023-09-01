// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/repoquery.hpp"

#include "common_options.hpp"

mamba::QueryType
str_to_qtype(const std::string& s)
{
    if (s == "search")
    {
        return mamba::QueryType::kSEARCH;
    }
    if (s == "depends")
    {
        return mamba::QueryType::kDEPENDS;
    }
    if (s == "whoneeds")
    {
        return mamba::QueryType::kWHONEEDS;
    }
    throw std::runtime_error("Could not parse query type");
}

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

    subcom->callback(
        [&]
        {
            using namespace mamba;

            auto qtype = str_to_qtype(query_type);
            QueryResultFormat format = QueryResultFormat::kTABLE;
            bool use_local = true;
            switch (qtype)
            {
                case QueryType::kSEARCH:
                    format = QueryResultFormat::kTABLE;
                    use_local = local > 0;  // use remote repodata by default for `search`
                    break;
                case QueryType::kDEPENDS:
                    format = QueryResultFormat::kTABLE;
                    use_local = (local == -1) || (local > 0);
                    break;
                case QueryType::kWHONEEDS:
                    format = QueryResultFormat::kTABLE;
                    use_local = (local == -1) || (local > 0);
                    break;
            }
            if (qtype == QueryType::kDEPENDS && recursive)
            {
                format = QueryResultFormat::kRECURSIVETABLE;
            }

            if (qtype == QueryType::kDEPENDS && show_as_tree)
            {
                format = QueryResultFormat::kTREE;
            }
            // Best guess to detect wildcard search; if there's no wildcard search, we want to show
            // the pretty single package view.
            if (qtype == QueryType::kSEARCH
                && (pretty_print || specs[0].find("*") == std::string::npos))
            {
                format = QueryResultFormat::kPRETTY;
            }

            if (config.at("json").compute().value<bool>())
            {
                format = QueryResultFormat::kJSON;
            }

            auto& channels = config.at("channels").compute().value<std::vector<std::string>>();
            use_local = use_local && channels.empty();

            repoquery(config, qtype, format, use_local, specs[0]);
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
