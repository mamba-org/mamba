// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <sstream>
#include <stack>

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <solv/evr.h>
#include <spdlog/spdlog.h>

#include "mamba/core/context.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/query.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"
#include "solv-cpp/queue.hpp"

namespace mamba
{
    namespace
    {
        void walk_graph(
            MPool pool,
            query_result::dependency_graph& dep_graph,
            query_result::dependency_graph::node_id parent,
            Solvable* s,
            std::map<Solvable*, size_t>& visited,
            std::map<std::string, size_t>& not_found,
            int depth = -1
        )
        {
            if (depth == 0)
            {
                return;
            }
            depth -= 1;

            if (s && s->requires)
            {
                Id* reqp = s->repo->idarraydata + s->requires;
                Id req = *reqp;

                while (req != 0)
                {
                    solv::ObjQueue rec_solvables = {};
                    // the following prints the requested version
                    solv::ObjQueue job = { SOLVER_SOLVABLE_PROVIDES, req };
                    selection_solvables(pool, job.raw(), rec_solvables.raw());

                    if (rec_solvables.size() != 0)
                    {
                        Solvable* rs = nullptr;
                        for (auto& el : rec_solvables)
                        {
                            rs = pool_id2solvable(pool, el);
                            if (rs->name == req)
                            {
                                break;
                            }
                        }
                        auto it = visited.find(rs);
                        if (it == visited.end())
                        {
                            auto pkg_info = pool.id2pkginfo(pool_solvable2id(pool, rs));
                            assert(pkg_info.has_value());
                            auto dep_id = dep_graph.add_node(std::move(pkg_info).value());
                            dep_graph.add_edge(parent, dep_id);
                            visited.insert(std::make_pair(rs, dep_id));
                            walk_graph(pool, dep_graph, dep_id, rs, visited, not_found, depth);
                        }
                        else
                        {
                            dep_graph.add_edge(parent, it->second);
                        }
                    }
                    else
                    {
                        std::string name = pool_id2str(pool, req);
                        auto it = not_found.find(name);
                        if (it == not_found.end())
                        {
                            auto dep_id = dep_graph.add_node(
                                PackageInfo(util::concat(name, " >>> NOT FOUND <<<"))
                            );
                            dep_graph.add_edge(parent, dep_id);
                            not_found.insert(std::make_pair(name, dep_id));
                        }
                        else
                        {
                            dep_graph.add_edge(parent, it->second);
                        }
                    }
                    ++reqp;
                    req = *reqp;
                }
            }
        }

        void reverse_walk_graph(
            MPool& pool,
            query_result::dependency_graph& dep_graph,
            query_result::dependency_graph::node_id parent,
            Solvable* s,
            std::map<Solvable*, size_t>& visited
        )
        {
            if (s)
            {
                // figure out who requires `s`
                solv::ObjQueue solvables = {};

                pool_whatmatchesdep(pool, SOLVABLE_REQUIRES, s->name, solvables.raw(), -1);

                if (solvables.size() != 0)
                {
                    for (auto& el : solvables)
                    {
                        ::Solvable* rs = pool_id2solvable(pool, el);
                        auto it = visited.find(rs);
                        if (it == visited.end())
                        {
                            auto pkg_info = pool.id2pkginfo(el);
                            assert(pkg_info.has_value());
                            auto dep_id = dep_graph.add_node(std::move(pkg_info).value());
                            dep_graph.add_edge(parent, dep_id);
                            visited.insert(std::make_pair(rs, dep_id));
                            reverse_walk_graph(pool, dep_graph, dep_id, rs, visited);
                        }
                        else
                        {
                            dep_graph.add_edge(parent, it->second);
                        }
                    }
                }
            }
        }
    }

    /************************
     * Query implementation *
     ************************/

    Query::Query(MPool& pool)
        : m_pool(pool)
    {
        m_pool.get().create_whatprovides();
    }

    namespace
    {
        auto print_solvable(const PackageInfo& pkg, const std::vector<PackageInfo>& otherVersions)
        {
            auto out = Console::stream();
            std::string header = fmt::format("{} {} {}", pkg.name, pkg.version, pkg.build_string);
            fmt::print(out, "{:^40}\n{:-^{}}\n\n", header, "", header.size() > 40 ? header.size() : 40);

            static constexpr const char* fmtstring = " {:<15} {}\n";
            fmt::print(out, fmtstring, "File Name", pkg.fn);
            fmt::print(out, fmtstring, "Name", pkg.name);
            fmt::print(out, fmtstring, "Version", pkg.version);
            fmt::print(out, fmtstring, "Build", pkg.build_string);
            fmt::print(out, fmtstring, "Build Number", pkg.build_number);
            fmt::print(out, " {:<15} {} Kb\n", "Size", pkg.size / 1000);
            fmt::print(out, fmtstring, "License", pkg.license);
            fmt::print(out, fmtstring, "Subdir", pkg.subdir);

            std::string url_remaining, url_scheme, url_auth, url_token;
            util::split_scheme_auth_token(pkg.url, url_remaining, url_scheme, url_auth, url_token);

            fmt::print(out, " {:<15} {}://{}\n", "URL", url_scheme, url_remaining);

            fmt::print(out, fmtstring, "MD5", pkg.md5.empty() ? "Not available" : pkg.md5);
            fmt::print(out, fmtstring, "SHA256", pkg.sha256.empty() ? "Not available" : pkg.sha256);
            if (!pkg.track_features.empty())
            {
                fmt::print(out, fmtstring, "Track Features", fmt::join(pkg.track_features, ","));
            }

            // std::cout << fmt::format<char>(
            // " {:<15} {:%Y-%m-%d %H:%M:%S} UTC\n", "Timestamp", fmt::gmtime(pkg.timestamp));

            if (!pkg.depends.empty())
            {
                fmt::print(out, "\n Dependencies:\n");
                for (auto& d : pkg.depends)
                {
                    fmt::print(out, "  - {}\n", d);
                }
            }

            if (!pkg.constrains.empty())
            {
                fmt::print(out, "\n Run Constraints:\n");
                for (auto& c : pkg.constrains)
                {
                    fmt::print(out, "  - {}\n", c);
                }
            }

            if (!pkg.depends.empty())
            {
                fmt::print(out, "\n Dependencies:\n");
                for (auto& d : pkg.depends)
                {
                    fmt::print(out, "  - {}\n", d);
                }
            }

            if (!otherVersions.empty())
            {
                std::map<std::string, std::vector<PackageInfo>> buildsByVersion;
                for (const auto& p : otherVersions)
                {
                    buildsByVersion[p.version].push_back(p);
                }
                fmt::print(out, "\n Other Versions ({}):\n\n", buildsByVersion.size());

                std::stringstream buffer;

                using namespace printers;
                Table printer({ "Version", "Build", "", "" });
                printer.set_alignment(
                    { alignment::left, alignment::left, alignment::left, alignment::right }
                );
                // We want the newest version to be on top, therefore we iterate in reverse.
                for (auto it = buildsByVersion.rbegin(); it != buildsByVersion.rend(); it++)
                {
                    std::vector<FormattedString> row;
                    row.push_back(it->second.front().version);
                    row.push_back(it->second.front().build_string);
                    if (it->second.size() > 1)
                    {
                        row.push_back("(+");
                        row.push_back(fmt::format("{} builds)", it->second.size() - 1));
                    }
                    else
                    {
                        row.push_back("");
                        row.push_back("");
                    }
                    printer.add_row(row);
                }
                printer.print(buffer);

                std::string line;
                while (std::getline(buffer, line))
                {
                    out << "  " << line << std::endl;
                }
            }

            out << '\n';
        }
    }

    query_result Query::find(const std::string& query) const
    {
        solv::ObjQueue job, solvables;

        const Id id = pool_conda_matchspec(m_pool.get(), query.c_str());
        if (!id)
        {
            throw std::runtime_error("Could not generate query for " + query);
        }
        job.push_back(SOLVER_SOLVABLE_PROVIDES, id);

        selection_solvables(m_pool.get(), job.raw(), solvables.raw());
        query_result::dependency_graph g;

        Pool* pool = m_pool.get();
        std::sort(
            solvables.begin(),
            solvables.end(),
            [pool](Id a, Id b)
            {
                Solvable* sa;
                Solvable* sb;
                sa = pool_id2solvable(pool, a);
                sb = pool_id2solvable(pool, b);
                return (pool_evrcmp(pool, sa->evr, sb->evr, EVRCMP_COMPARE) > 0);
            }
        );

        for (auto& el : solvables)
        {
            auto pkg_info = m_pool.get().id2pkginfo(el);
            assert(pkg_info.has_value());
            g.add_node(std::move(pkg_info).value());
        }

        return query_result(QueryType::kSEARCH, query, std::move(g));
    }

    query_result Query::whoneeds(const std::string& query, bool tree) const
    {
        const Id id = pool_conda_matchspec(m_pool.get(), query.c_str());
        if (!id)
        {
            throw std::runtime_error("Could not generate query for " + query);
        }

        solv::ObjQueue job = { SOLVER_SOLVABLE_PROVIDES, id };
        query_result::dependency_graph g;

        if (tree)
        {
            solv::ObjQueue solvables = {};
            selection_solvables(m_pool.get(), job.raw(), solvables.raw());
            if (!solvables.empty())
            {
                auto pkg_info = m_pool.get().id2pkginfo(solvables.front());
                assert(pkg_info.has_value());
                const auto node_id = g.add_node(std::move(pkg_info).value());
                Solvable* const latest = pool_id2solvable(m_pool.get(), solvables.front());
                std::map<Solvable*, size_t> visited = { { latest, node_id } };
                reverse_walk_graph(m_pool, g, node_id, latest, visited);
            }
        }
        else
        {
            solv::ObjQueue solvables = {};
            pool_whatmatchesdep(m_pool.get(), SOLVABLE_REQUIRES, id, solvables.raw(), -1);
            for (auto& el : solvables)
            {
                auto pkg_info = m_pool.get().id2pkginfo(el);
                assert(pkg_info.has_value());
                g.add_node(std::move(pkg_info).value());
            }
        }
        return query_result(QueryType::kWHONEEDS, query, std::move(g));
    }

    query_result Query::depends(const std::string& query, bool tree) const
    {
        solv::ObjQueue job, solvables;

        const Id id = pool_conda_matchspec(m_pool.get(), query.c_str());
        if (!id)
        {
            throw std::runtime_error("Could not generate query for " + query);
        }
        job.push_back(SOLVER_SOLVABLE_PROVIDES, id);

        query_result::dependency_graph g;
        selection_solvables(m_pool.get(), job.raw(), solvables.raw());

        int depth = tree ? -1 : 1;

        auto find_latest_in_non_empty = [&](solv::ObjQueue& lsolvables) -> Solvable*
        {
            ::Solvable* latest = pool_id2solvable(m_pool.get(), lsolvables.front());
            for (Id const solv : solvables)
            {
                Solvable* s = pool_id2solvable(m_pool.get(), solv);
                if (pool_evrcmp(m_pool.get(), s->evr, latest->evr, 0) > 0)
                {
                    latest = s;
                }
            }
            return latest;
        };

        if (!solvables.empty())
        {
            ::Solvable* const latest = find_latest_in_non_empty(solvables);
            auto pkg_info = m_pool.get().id2pkginfo(pool_solvable2id(m_pool.get(), latest));
            assert(pkg_info.has_value());
            const auto node_id = g.add_node(std::move(pkg_info).value());

            std::map<Solvable*, size_t> visited = { { latest, node_id } };
            std::map<std::string, size_t> not_found;
            walk_graph(m_pool, g, node_id, latest, visited, not_found, depth);
        }

        return query_result(QueryType::kDEPENDS, query, std::move(g));
    }

    /*******************************
     * query_result implementation *
     *******************************/

    query_result::query_result(QueryType type, const std::string& query, dependency_graph&& dep_graph)
        : m_type(type)
        , m_query(query)
        , m_dep_graph(std::move(dep_graph))
    {
        reset_pkg_view_list();
    }

    QueryType query_result::query_type() const
    {
        return m_type;
    }

    const std::string& query_result::query() const
    {
        return m_query;
    }

    query_result& query_result::sort(std::string field)
    {
        auto compare_ids = [&, fun = PackageInfo::less(field)](node_id lhs, node_id rhs)
        { return fun(m_dep_graph.node(lhs), m_dep_graph.node(rhs)); };

        if (!m_ordered_pkg_id_list.empty())
        {
            for (auto& [_, pkg_id_list] : m_ordered_pkg_id_list)
            {
                std::sort(pkg_id_list.begin(), pkg_id_list.end(), compare_ids);
            }
        }
        else
        {
            std::sort(m_pkg_id_list.begin(), m_pkg_id_list.end(), compare_ids);
        }

        return *this;
    }

    query_result& query_result::groupby(std::string field)
    {
        auto fun = PackageInfo::get_field_getter(field);
        if (m_ordered_pkg_id_list.empty())
        {
            for (auto& id : m_pkg_id_list)
            {
                m_ordered_pkg_id_list[fun(m_dep_graph.node(id))].push_back(id);
            }
        }
        else
        {
            ordered_package_list tmp;
            for (auto& entry : m_ordered_pkg_id_list)
            {
                for (auto& id : entry.second)
                {
                    std::string key = entry.first + '/' + fun(m_dep_graph.node(id));
                    tmp[std::move(key)].push_back(id);
                }
            }
            m_ordered_pkg_id_list = std::move(tmp);
        }
        return *this;
    }

    query_result& query_result::reset()
    {
        reset_pkg_view_list();
        m_ordered_pkg_id_list.clear();
        return *this;
    }

    std::ostream& query_result::table(std::ostream& out) const
    {
        return table(out, { "Name", "Version", "Build", "", "", "Channel", "Subdir" });
    }

    namespace
    {
        /** Remove potential subdir from channel name (not url!). */
        auto cut_subdir(std::string_view str) -> std::string
        {
            return util::split(str, "/", 1).front();  // Has at least one element
        }

        /** Get subdir from channel name. */
        auto get_subdir(std::string_view str) -> std::string
        {
            return util::split(str, "/").back();
        }

    }

    std::ostream& query_result::table(std::ostream& out, const std::vector<std::string>& fmt) const
    {
        if (m_pkg_id_list.empty())
        {
            out << "No entries matching \"" << m_query << "\" found" << std::endl;
        }

        std::vector<mamba::printers::FormattedString> headers;
        std::vector<std::string> cmds, args;
        std::vector<mamba::printers::alignment> alignments;
        for (auto& f : fmt)
        {
            if (f.find_first_of(":") == f.npos)
            {
                headers.push_back(f);
                cmds.push_back(f);
                args.push_back("");
            }
            else
            {
                auto sfmt = util::split(f, ":", 1);
                headers.push_back(sfmt[0]);
                cmds.push_back(sfmt[0]);
                args.push_back(sfmt[1]);
            }
            alignments.push_back(f == "" ? printers::alignment::right : printers::alignment::left);
        }

        auto format_row = [&](const PackageInfo& pkg, const std::vector<PackageInfo>& builds)
        {
            std::vector<mamba::printers::FormattedString> row;
            for (std::size_t i = 0; i < cmds.size(); ++i)
            {
                const auto& cmd = cmds[i];
                if (cmd == "Name")
                {
                    row.push_back(pkg.name);
                }
                else if (cmd == "Version")
                {
                    row.push_back(pkg.version);
                }
                else if (cmd == "Build")
                {
                    row.push_back(pkg.build_string);
                    if (builds.size() > 1)
                    {
                        row.push_back("(+");
                        row.push_back(fmt::format("{} builds)", builds.size() - 1));
                    }
                    else
                    {
                        row.push_back("");
                        row.push_back("");
                    }
                }
                else if (cmd == "Channel")
                {
                    row.push_back(cut_subdir(cut_repo_name(pkg.channel)));
                }
                else if (cmd == "Subdir")
                {
                    row.push_back(get_subdir(pkg.channel));
                }
                else if (cmd == "Depends")
                {
                    std::string depends_qualifier;
                    for (const auto& dep : pkg.depends)
                    {
                        if (util::starts_with(dep, args[i]))
                        {
                            depends_qualifier = dep;
                            break;
                        }
                    }
                    row.push_back(depends_qualifier);
                }
            }
            return row;
        };

        printers::Table printer(headers);
        printer.set_alignment(alignments);

        if (!m_ordered_pkg_id_list.empty())
        {
            std::map<std::string, std::map<std::string, std::vector<PackageInfo>>> packageBuildsByVersion;
            for (auto& entry : m_ordered_pkg_id_list)
            {
                for (const auto& id : entry.second)
                {
                    auto package = m_dep_graph.node(id);
                    packageBuildsByVersion[package.name][package.version].push_back(package);
                }
            }

            for (const auto& entry : packageBuildsByVersion)
            {
                // We want the newest version to be on top, therefore we iterate in reverse.
                for (auto it = entry.second.rbegin(); it != entry.second.rend(); ++it)
                {
                    printer.add_row(format_row(it->second[0], it->second));
                }
            }
        }
        else
        {
            for (const auto& id : m_pkg_id_list)
            {
                printer.add_row(format_row(m_dep_graph.node(id), {}));
            }
        }

        return printer.print(out);
    }

    class graph_printer
    {
    public:

        using graph_type = query_result::dependency_graph;
        using node_id = graph_type::node_id;

        explicit graph_printer(std::ostream& out)
            : m_is_last(false)
            , m_out(out)
        {
        }

        void start_node(node_id node, const graph_type& g)
        {
            print_prefix(node);
            m_out << get_package_repr(g.node(node)) << '\n';
            if (node == 0u)
            {
                m_prefix_stack.push_back("  ");
            }
            else if (is_on_last_stack(node))
            {
                m_prefix_stack.push_back("   ");
            }
            else
            {
                m_prefix_stack.push_back("│  ");
            }
        }

        void finish_node(node_id /*node*/, const graph_type&)
        {
            m_prefix_stack.pop_back();
        }

        void start_edge(node_id from, node_id to, const graph_type& g)
        {
            m_is_last = g.successors(from).back() == to;
            if (m_is_last)
            {
                m_last_stack.push(to);
            }
        }

        void tree_edge(node_id, node_id, const graph_type&)
        {
        }
        void back_edge(node_id, node_id, const graph_type&)
        {
        }
        void forward_or_cross_edge(node_id, node_id to, const graph_type& g)
        {
            print_prefix(to);
            m_out << g.node(to).name
                  << fmt::format(Context::instance().graphics_params.palette.shown, " already visited\n");
        }

        void finish_edge(node_id /*from*/, node_id to, const graph_type& /*g*/)
        {
            if (is_on_last_stack(to))
            {
                m_last_stack.pop();
            }
        }

    private:

        bool is_on_last_stack(node_id node) const
        {
            return !m_last_stack.empty() && m_last_stack.top() == node;
        }

        void print_prefix(node_id node)
        {
            for (const auto& token : m_prefix_stack)
            {
                m_out << token;
            }
            if (node != node_id(0))
            {
                m_out << (m_is_last ? "└─ " : "├─ ");
            }
        }

        std::string get_package_repr(const PackageInfo& pkg) const
        {
            return pkg.version.empty() ? pkg.name : pkg.name + '[' + pkg.version + ']';
        }

        std::stack<node_id> m_last_stack;
        std::vector<std::string> m_prefix_stack;
        bool m_is_last;
        std::ostream& m_out;
    };

    std::ostream& query_result::tree(std::ostream& out) const
    {
        bool use_graph = (m_dep_graph.number_of_nodes() > 0) && !m_dep_graph.successors(0).empty();
        if (use_graph)
        {
            graph_printer printer(out);
            dfs_raw(m_dep_graph, printer, /* start= */ node_id(0));
        }
        else if (!m_pkg_id_list.empty())
        {
            out << m_query << '\n';
            for (size_t i = 0; i < m_pkg_id_list.size() - 1; ++i)
            {
                out << "  ├─ " << get_package_repr(m_dep_graph.node(m_pkg_id_list[i])) << '\n';
            }
            out << "  └─ " << get_package_repr(m_dep_graph.node(m_pkg_id_list.back())) << '\n';
        }

        return out;
    }

    nlohmann::json query_result::json(ChannelContext& channel_context) const
    {
        nlohmann::json j;
        std::string query_type = m_type == QueryType::kSEARCH
                                     ? "search"
                                     : (m_type == QueryType::kDEPENDS ? "depends" : "whoneeds");
        j["query"] = { { "query", MatchSpec{ m_query, channel_context }.conda_build_form() },
                       { "type", query_type } };

        std::string msg = m_pkg_id_list.empty() ? "No entries matching \"" + m_query + "\" found"
                                                : "";
        j["result"] = { { "msg", msg }, { "status", "OK" } };

        j["result"]["pkgs"] = nlohmann::json::array();
        for (size_t i = 0; i < m_pkg_id_list.size(); ++i)
        {
            auto pkg_info_json = m_dep_graph.node(m_pkg_id_list[i]).json_record();
            // We want the cannonical channel name here.
            // We do not know what is in the `channel` field so we need to make sure.
            // This is most likely legacy and should be updated on the next major release.
            pkg_info_json["channel"] = cut_subdir(cut_repo_name(pkg_info_json["channel"]));
            j["result"]["pkgs"].push_back(std::move(pkg_info_json));
        }

        if (m_type != QueryType::kSEARCH && !m_pkg_id_list.empty())
        {
            j["result"]["graph_roots"] = nlohmann::json::array();
            if (!m_dep_graph.successors(0).empty())
            {
                auto pkg_info_json = m_dep_graph.node(0).json_record();
                // We want the cannonical channel name here.
                // We do not know what is in the `channel` field so we need to make sure.
                // This is most likely legacy and should be updated on the next major release.
                pkg_info_json["channel"] = cut_subdir(cut_repo_name(pkg_info_json["channel"]));
                j["result"]["graph_roots"].push_back(std::move(pkg_info_json));
            }
            else
            {
                j["result"]["graph_roots"].push_back(nlohmann::json(m_query));
            }
        }
        return j;
    }

    std::ostream& query_result::pretty(std::ostream& out) const
    {
        if (m_pkg_id_list.empty())
        {
            out << "No entries matching \"" << m_query << "\" found" << std::endl;
        }
        else
        {
            std::map<std::string, std::vector<PackageInfo>> packages;
            for (const auto& id : m_pkg_id_list)
            {
                auto package = m_dep_graph.node(id);
                packages[package.name].push_back(package);
            }

            for (const auto& entry : packages)
            {
                print_solvable(
                    entry.second[0],
                    std::vector(entry.second.begin() + 1, entry.second.end())
                );
            }
        }
        return out;
    }

    bool query_result::empty() const
    {
        return m_dep_graph.empty();
    }

    void query_result::reset_pkg_view_list()
    {
        m_pkg_id_list.clear();
        m_pkg_id_list.reserve(m_dep_graph.number_of_nodes());
        m_dep_graph.for_each_node_id([&](node_id id) { m_pkg_id_list.push_back(id); });
    }

    std::string query_result::get_package_repr(const PackageInfo& pkg) const
    {
        return pkg.version.empty() ? pkg.name : fmt::format("{}[{}]", pkg.name, pkg.version);
    }
}  // namespace mamba
