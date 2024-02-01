// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <sstream>
#include <stack>
#include <unordered_set>

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <solv/evr.h>
#include <solv/pool.h>
#include <solv/repo.h>
#include <solv/selection.h>
#include <solv/solver.h>
#include <spdlog/spdlog.h>
extern "C"  // Incomplete header
{
#include <solv/conda.h>
}

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/query.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"
#include "solv-cpp/queue.hpp"

namespace mamba
{
    namespace
    {

        auto get_package_repr(const specs::PackageInfo& pkg) -> std::string
        {
            return pkg.version.empty() ? pkg.name : fmt::format("{}[{}]", pkg.name, pkg.version);
        }

        void walk_graph(
            MPool pool,
            QueryResult::dependency_graph& dep_graph,
            QueryResult::dependency_graph::node_id parent,
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
                                specs::PackageInfo(util::concat(name, " >>> NOT FOUND <<<"))
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
            QueryResult::dependency_graph& dep_graph,
            QueryResult::dependency_graph::node_id parent,
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

    namespace
    {
        /**
         * Prints metadata for a given package.
         */
        auto print_metadata(std::ostream& out, const specs::PackageInfo& pkg)
        {
            static constexpr const char* fmtstring = " {:<15} {}\n";
            fmt::print(out, fmtstring, "Name", pkg.name);
            fmt::print(out, fmtstring, "Version", pkg.version);
            fmt::print(out, fmtstring, "Build", pkg.build_string);
            fmt::print(out, " {:<15} {} kB\n", "Size", pkg.size / 1000);
            fmt::print(out, fmtstring, "License", pkg.license);
            fmt::print(out, fmtstring, "Subdir", pkg.subdir);
            fmt::print(out, fmtstring, "File Name", pkg.filename);

            using CondaURL = typename specs::CondaURL;
            auto url = CondaURL::parse(pkg.package_url);
            fmt::print(
                out,
                " {:<15} {}\n",
                "URL",
                url.pretty_str(CondaURL::StripScheme::no, '/', CondaURL::Credentials::Hide)
            );

            fmt::print(out, fmtstring, "MD5", pkg.md5.empty() ? "Not available" : pkg.md5);
            fmt::print(out, fmtstring, "SHA256", pkg.sha256.empty() ? "Not available" : pkg.sha256);
            if (!pkg.track_features.empty())
            {
                fmt::print(out, fmtstring, "Track Features", fmt::join(pkg.track_features, ","));
            }

            // std::cout << fmt::format<char>(
            // " {:<15} {:%Y-%m-%d %H:%M:%S} UTC\n", "Timestamp", fmt::gmtime(pkg.timestamp));

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
        }

        /**
         * Prints all other versions/builds in a table format for a given package.
         */
        auto print_other_builds(
            std::ostream& out,
            const specs::PackageInfo& pkg,
            const std::map<std::string, std::vector<specs::PackageInfo>> groupedOtherBuilds,
            bool showAllBuilds
        )
        {
            fmt::print(
                out,
                "\n Other {} ({}):\n\n",
                showAllBuilds ? "Builds" : "Versions",
                groupedOtherBuilds.size()
            );

            std::stringstream buffer;

            using namespace printers;
            Table printer({ "Version", "Build", "", "" });
            printer.set_alignment(
                { alignment::left, alignment::left, alignment::left, alignment::right }
            );
            bool collapseVersions = !showAllBuilds && groupedOtherBuilds.size() > 5;
            size_t counter = 0;
            // We want the newest version to be on top, therefore we iterate in reverse.
            for (auto it = groupedOtherBuilds.rbegin(); it != groupedOtherBuilds.rend(); it++)
            {
                ++counter;
                if (collapseVersions)
                {
                    if (counter == 3)
                    {
                        printer.add_row(
                            { "...",
                              fmt::format("({} hidden versions)", groupedOtherBuilds.size() - 4),
                              "",
                              "..." }
                        );
                        continue;
                    }
                    else if (counter > 3 && counter < groupedOtherBuilds.size() - 1)
                    {
                        continue;
                    }
                }

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
                out << " " << line << std::endl;
            }
        }

        /**
         * Prints detailed information about a given package, including a list of other
         * versions/builds.
         */
        auto print_solvable(
            std::ostream& out,
            const specs::PackageInfo& pkg,
            const std::vector<specs::PackageInfo>& otherBuilds,
            bool showAllBuilds
        )
        {
            // Filter and group builds/versions.
            std::map<std::string, std::vector<specs::PackageInfo>> groupedOtherBuilds;
            auto numOtherBuildsForLatestVersion = 0;
            if (showAllBuilds)
            {
                for (const auto& p : otherBuilds)
                {
                    if (p.sha256 != pkg.sha256)
                    {
                        groupedOtherBuilds[p.version + p.sha256].push_back(p);
                    }
                }
            }
            else
            {
                std::unordered_set<std::string> distinctBuildSHAs;
                for (const auto& p : otherBuilds)
                {
                    if (distinctBuildSHAs.insert(p.sha256).second)
                    {
                        if (p.version != pkg.version)
                        {
                            groupedOtherBuilds[p.version].push_back(p);
                        }
                        else
                        {
                            ++numOtherBuildsForLatestVersion;
                        }
                    }
                }
            }

            // Construct and print header line.
            std::string additionalBuilds;
            if (numOtherBuildsForLatestVersion > 0)
            {
                additionalBuilds = fmt::format(" (+ {} builds)", numOtherBuildsForLatestVersion);
            }
            std::string header = fmt::format("{} {} {}", pkg.name, pkg.version, pkg.build_string)
                                 + additionalBuilds;
            fmt::print(out, "{:^40}\n{:─^{}}\n\n", header, "", header.size() > 40 ? header.size() : 40);

            // Print metadata.
            print_metadata(out, pkg);

            if (!groupedOtherBuilds.empty())
            {
                print_other_builds(out, pkg, groupedOtherBuilds, showAllBuilds);
            }

            out << '\n';
        }
    }

    auto Query::find(MPool& mpool, const std::vector<std::string>& queries) -> QueryResult
    {
        solv::ObjQueue job, solvables;

        for (const auto& query : queries)
        {
            const Id id = pool_conda_matchspec(mpool, query.c_str());
            if (!id)
            {
                throw std::runtime_error("Could not generate query for " + query);
            }
            job.push_back(SOLVER_SOLVABLE_PROVIDES, id);
        }

        selection_solvables(mpool, job.raw(), solvables.raw());
        QueryResult::dependency_graph g;

        Pool* pool = mpool;
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
            auto pkg_info = mpool.id2pkginfo(el);
            assert(pkg_info.has_value());
            g.add_node(std::move(pkg_info).value());
        }

        return { QueryType::Search,
                 fmt::format("{}", fmt::join(queries, " ")),  // Yes this is disgusting
                 std::move(g) };
    }

    auto Query::whoneeds(MPool& mpool, const std::string& query, bool tree) -> QueryResult
    {
        const Id id = pool_conda_matchspec(mpool, query.c_str());
        if (!id)
        {
            throw std::runtime_error("Could not generate query for " + query);
        }

        solv::ObjQueue job = { SOLVER_SOLVABLE_PROVIDES, id };
        QueryResult::dependency_graph g;

        if (tree)
        {
            solv::ObjQueue solvables = {};
            selection_solvables(mpool, job.raw(), solvables.raw());
            if (!solvables.empty())
            {
                auto pkg_info = mpool.id2pkginfo(solvables.front());
                assert(pkg_info.has_value());
                const auto node_id = g.add_node(std::move(pkg_info).value());
                Solvable* const latest = pool_id2solvable(mpool, solvables.front());
                std::map<Solvable*, size_t> visited = { { latest, node_id } };
                reverse_walk_graph(mpool, g, node_id, latest, visited);
            }
        }
        else
        {
            solv::ObjQueue solvables = {};
            pool_whatmatchesdep(mpool, SOLVABLE_REQUIRES, id, solvables.raw(), -1);
            for (auto& el : solvables)
            {
                auto pkg_info = mpool.id2pkginfo(el);
                assert(pkg_info.has_value());
                g.add_node(std::move(pkg_info).value());
            }
        }
        return { QueryType::WhoNeeds, query, std::move(g) };
    }

    auto Query::depends(MPool& mpool, const std::string& query, bool tree) -> QueryResult
    {
        solv::ObjQueue job, solvables;

        const Id id = pool_conda_matchspec(mpool, query.c_str());
        if (!id)
        {
            throw std::runtime_error("Could not generate query for " + query);
        }
        job.push_back(SOLVER_SOLVABLE_PROVIDES, id);

        QueryResult::dependency_graph g;
        selection_solvables(mpool, job.raw(), solvables.raw());

        int depth = tree ? -1 : 1;

        auto find_latest_in_non_empty = [&](solv::ObjQueue& lsolvables) -> Solvable*
        {
            ::Solvable* latest = pool_id2solvable(mpool, lsolvables.front());
            for (Id const solv : solvables)
            {
                Solvable* s = pool_id2solvable(mpool, solv);
                if (pool_evrcmp(mpool, s->evr, latest->evr, 0) > 0)
                {
                    latest = s;
                }
            }
            return latest;
        };

        if (!solvables.empty())
        {
            ::Solvable* const latest = find_latest_in_non_empty(solvables);
            auto pkg_info = mpool.id2pkginfo(pool_solvable2id(mpool, latest));
            assert(pkg_info.has_value());
            const auto node_id = g.add_node(std::move(pkg_info).value());

            std::map<Solvable*, size_t> visited = { { latest, node_id } };
            std::map<std::string, size_t> not_found;
            walk_graph(mpool, g, node_id, latest, visited, not_found, depth);
        }

        return { QueryType::Depends, query, std::move(g) };
    }

    /******************************
     *  QueryType implementation  *
     ******************************/

    auto query_type_parse(std::string_view name) -> QueryType
    {
        auto l_name = util::to_lower(name);
        if (l_name == "search")
        {
            return QueryType::Search;
        }
        if (l_name == "depends")
        {
            return QueryType::Depends;
        }
        if (l_name == "whoneeds")
        {
            return QueryType::WhoNeeds;
        }
        throw std::invalid_argument(fmt::format("Invalid enum name \"{}\"", name));
    }

    /*******************************
     * query_result implementation *
     *******************************/

    QueryResult::QueryResult(QueryType type, std::string query, dependency_graph dep_graph)
        : m_type(type)
        , m_query(std::move(query))
        , m_dep_graph(std::move(dep_graph))
    {
        reset_pkg_view_list();
    }

    auto QueryResult::type() const -> QueryType
    {
        return m_type;
    }

    auto QueryResult::query() const -> const std::string&
    {
        return m_query;
    }

    auto QueryResult::sort(std::string_view field) -> QueryResult&
    {
        auto compare_ids = [&](node_id lhs, node_id rhs)
        { return m_dep_graph.node(lhs).field(field) < m_dep_graph.node(rhs).field(field); };

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

    auto QueryResult::groupby(std::string_view field) -> QueryResult&
    {
        if (m_ordered_pkg_id_list.empty())
        {
            for (auto& id : m_pkg_id_list)
            {
                m_ordered_pkg_id_list[m_dep_graph.node(id).field(field)].push_back(id);
            }
        }
        else
        {
            ordered_package_list tmp;
            for (auto& entry : m_ordered_pkg_id_list)
            {
                for (auto& id : entry.second)
                {
                    std::string key = entry.first + '/' + m_dep_graph.node(id).field(field);
                    tmp[std::move(key)].push_back(id);
                }
            }
            m_ordered_pkg_id_list = std::move(tmp);
        }
        return *this;
    }

    auto QueryResult::reset() -> QueryResult&
    {
        reset_pkg_view_list();
        m_ordered_pkg_id_list.clear();
        return *this;
    }

    auto QueryResult::table(std::ostream& out) const -> std::ostream&
    {
        return table(
            out,
            { "Name",
              "Version",
              "Build",
              printers::alignmentMarker(printers::alignment::left),
              printers::alignmentMarker(printers::alignment::right),
              "Channel",
              "Subdir" }
        );
    }

    auto QueryResult::table_to_str() const -> std::string
    {
        auto ss = std::stringstream();
        table(ss);
        return ss.str();
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

    auto QueryResult::table(std::ostream& out, const std::vector<std::string_view>& columns) const
        -> std::ostream&
    {
        if (m_pkg_id_list.empty())
        {
            out << "No entries matching \"" << m_query << "\" found" << std::endl;
        }

        std::vector<mamba::printers::FormattedString> headers;
        std::vector<std::string_view> cmds, args;
        std::vector<mamba::printers::alignment> alignments;
        for (auto& col : columns)
        {
            if (col == printers::alignmentMarker(printers::alignment::right)
                || col == printers::alignmentMarker(printers::alignment::left))
            {
                // If an alignment marker is passed, we remove the column name.
                headers.emplace_back("");
                cmds.emplace_back("");
                args.emplace_back("");
                // We only check for the right alignment marker, as left alignment is set the
                // default.
                if (col == printers::alignmentMarker(printers::alignment::right))
                {
                    alignments.push_back(printers::alignment::right);
                    continue;
                }
            }
            else if (col.find_first_of(":") == col.npos)
            {
                headers.emplace_back(col);
                cmds.push_back(col);
                args.emplace_back("");
            }
            else
            {
                auto sfmt = util::split(col, ":", 1);
                headers.emplace_back(sfmt[0]);
                cmds.push_back(sfmt[0]);
                args.push_back(sfmt[1]);
            }
            // By default, columns are left aligned.
            alignments.push_back(printers::alignment::left);
        }

        auto format_row =
            [&](const specs::PackageInfo& pkg, const std::vector<specs::PackageInfo>& builds)
        {
            std::vector<mamba::printers::FormattedString> row;
            for (std::size_t i = 0; i < cmds.size(); ++i)
            {
                const auto& cmd = cmds[i];
                if (cmd == "Name")
                {
                    row.emplace_back(pkg.name);
                }
                else if (cmd == "Version")
                {
                    row.emplace_back(pkg.version);
                }
                else if (cmd == "Build")
                {
                    row.emplace_back(pkg.build_string);
                    if (builds.size() > 1)
                    {
                        row.emplace_back("(+");
                        row.emplace_back(fmt::format("{} builds)", builds.size() - 1));
                    }
                    else
                    {
                        row.emplace_back("");
                        row.emplace_back("");
                    }
                }
                else if (cmd == "Channel")
                {
                    row.emplace_back(cut_subdir(cut_repo_name(pkg.channel)));
                }
                else if (cmd == "Subdir")
                {
                    row.emplace_back(get_subdir(pkg.channel));
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
                    row.emplace_back(depends_qualifier);
                }
            }
            return row;
        };

        printers::Table printer(headers);
        printer.set_alignment(alignments);

        if (!m_ordered_pkg_id_list.empty())
        {
            std::map<std::string, std::map<std::string, std::vector<specs::PackageInfo>>>
                packageBuildsByVersion;
            std::unordered_set<std::string> distinctBuildSHAs;
            for (auto& entry : m_ordered_pkg_id_list)
            {
                for (const auto& id : entry.second)
                {
                    auto package = m_dep_graph.node(id);
                    if (distinctBuildSHAs.insert(package.sha256).second)
                    {
                        packageBuildsByVersion[package.name][package.version].push_back(package);
                    }
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

    using GraphicsParams = Context::GraphicsParams;

    class graph_printer
    {
    public:

        using graph_type = QueryResult::dependency_graph;
        using node_id = graph_type::node_id;

        explicit graph_printer(std::ostream& out, GraphicsParams graphics)
            : m_is_last(false)
            , m_out(out)
            , m_graphics(std::move(graphics))
        {
        }

        void start_node(node_id node, const graph_type& g)
        {
            print_prefix(node);
            m_out << get_package_repr(g.node(node)) << '\n';
            if (node == 0u)
            {
                m_prefix_stack.emplace_back("  ");
            }
            else if (is_on_last_stack(node))
            {
                m_prefix_stack.emplace_back("   ");
            }
            else
            {
                m_prefix_stack.emplace_back("│  ");
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
            m_out << g.node(to).name << fmt::format(m_graphics.palette.shown, " already visited\n");
        }

        void finish_edge(node_id /*from*/, node_id to, const graph_type& /*g*/)
        {
            if (is_on_last_stack(to))
            {
                m_last_stack.pop();
            }
        }

    private:

        [[nodiscard]] auto is_on_last_stack(node_id node) const -> bool
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

        [[nodiscard]] auto get_package_repr(const specs::PackageInfo& pkg) const -> std::string
        {
            return pkg.version.empty() ? pkg.name : pkg.name + '[' + pkg.version + ']';
        }

        std::stack<node_id> m_last_stack;
        std::vector<std::string> m_prefix_stack;
        bool m_is_last;
        std::ostream& m_out;
        const GraphicsParams m_graphics;
    };

    auto QueryResult::tree(std::ostream& out, const GraphicsParams& graphics) const -> std::ostream&
    {
        bool use_graph = (m_dep_graph.number_of_nodes() > 0) && !m_dep_graph.successors(0).empty();
        if (use_graph)
        {
            graph_printer printer{ out, graphics };
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

    auto QueryResult::tree_to_str(const Context::GraphicsParams& params) const -> std::string
    {
        auto ss = std::stringstream();
        tree(ss, params);
        return ss.str();
    }

    auto QueryResult::json() const -> nlohmann::json
    {
        nlohmann::json j;
        std::string query_type = std::string(util::to_lower(enum_name(m_type)));
        j["query"] = { { "query", m_query }, { "type", query_type } };

        std::string msg = m_pkg_id_list.empty() ? "No entries matching \"" + m_query + "\" found"
                                                : "";
        j["result"] = { { "msg", msg }, { "status", "OK" } };

        j["result"]["pkgs"] = nlohmann::json::array();
        for (auto id : m_pkg_id_list)
        {
            nlohmann::json pkg_info_json = m_dep_graph.node(id);
            // We want the cannonical channel name here.
            // We do not know what is in the `channel` field so we need to make sure.
            // This is most likely legacy and should be updated on the next major release.
            pkg_info_json["channel"] = cut_subdir(
                cut_repo_name(pkg_info_json["channel"].get<std::string_view>())
            );
            j["result"]["pkgs"].push_back(std::move(pkg_info_json));
        }

        if (m_type != QueryType::Search && !m_pkg_id_list.empty())
        {
            j["result"]["graph_roots"] = nlohmann::json::array();
            if (!m_dep_graph.successors(0).empty())
            {
                nlohmann::json pkg_info_json = m_dep_graph.node(0);
                // We want the cannonical channel name here.
                // We do not know what is in the `channel` field so we need to make sure.
                // This is most likely legacy and should be updated on the next major release.
                pkg_info_json["channel"] = cut_subdir(
                    cut_repo_name(pkg_info_json["channel"].get<std::string_view>())
                );
                j["result"]["graph_roots"].push_back(std::move(pkg_info_json));
            }
            else
            {
                j["result"]["graph_roots"].push_back(nlohmann::json(m_query));
            }
        }
        return j;
    }

    auto QueryResult::pretty(std::ostream& out, bool show_all_builds) const -> std::ostream&
    {
        if (m_pkg_id_list.empty())
        {
            out << "No entries matching \"" << m_query << "\" found" << std::endl;
        }
        else
        {
            std::map<std::string, std::vector<specs::PackageInfo>> packages;
            for (const auto& id : m_pkg_id_list)
            {
                auto package = m_dep_graph.node(id);
                packages[package.name].push_back(package);
            }

            for (const auto& entry : packages)
            {
                print_solvable(
                    out,
                    entry.second[0],
                    std::vector(entry.second.begin() + 1, entry.second.end()),
                    show_all_builds
                );
            }
        }
        return out;
    }

    auto QueryResult::pretty_to_str(bool show_all_builds) const -> std::string
    {
        auto ss = std::stringstream();
        pretty(ss, show_all_builds);
        return ss.str();
    }

    auto QueryResult::empty() const -> bool
    {
        return m_dep_graph.empty();
    }

    void QueryResult::reset_pkg_view_list()
    {
        m_pkg_id_list.clear();
        m_pkg_id_list.reserve(m_dep_graph.number_of_nodes());
        m_dep_graph.for_each_node_id([&](node_id id) { m_pkg_id_list.push_back(id); });
    }
}
