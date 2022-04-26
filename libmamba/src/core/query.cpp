// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

extern "C"
{
#include <solv/evr.h>
}

#include <iostream>
#include <stack>

#include "mamba/core/query.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{
    void walk_graph(query_result::dependency_graph& dep_graph,
                    query_result::dependency_graph::node_id parent,
                    Solvable* s,
                    std::map<Solvable*, size_t>& visited,
                    std::map<std::string, size_t>& not_found,
                    int depth = -1)
    {
        if (depth == 0)
        {
            return;
        }
        depth -= 1;

        if (s && s->requires)
        {
            auto* pool = s->repo->pool;

            Id* reqp = s->repo->idarraydata + s->requires;
            Id req = *reqp;

            while (req != 0)
            {
                Queue job, rec_solvables;
                queue_init(&rec_solvables);
                queue_init(&job);
                // the following prints the requested version
                queue_push2(&job, SOLVER_SOLVABLE_PROVIDES, req);
                selection_solvables(pool, &job, &rec_solvables);

                if (rec_solvables.count != 0)
                {
                    Solvable* rs = nullptr;
                    for (int i = 0; i < rec_solvables.count; i++)
                    {
                        rs = pool_id2solvable(pool, rec_solvables.elements[i]);
                        if (rs->name == req)
                        {
                            break;
                        }
                    }
                    auto it = visited.find(rs);
                    if (it == visited.end())
                    {
                        auto dep_id = dep_graph.add_node(PackageInfo(rs));
                        dep_graph.add_edge(parent, dep_id);
                        visited.insert(std::make_pair(rs, dep_id));
                        walk_graph(dep_graph, dep_id, rs, visited, not_found, depth);
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
                        auto dep_id
                            = dep_graph.add_node(PackageInfo(concat(name, " >>> NOT FOUND <<<")));
                        dep_graph.add_edge(parent, dep_id);
                        not_found.insert(std::make_pair(name, dep_id));
                    }
                    else
                    {
                        dep_graph.add_edge(parent, it->second);
                    }
                }
                queue_free(&rec_solvables);
                ++reqp;
                req = *reqp;
            }
        }
    }

    void reverse_walk_graph(query_result::dependency_graph& dep_graph,
                            query_result::dependency_graph::node_id parent,
                            Solvable* s,
                            std::map<Solvable*, size_t>& visited)
    {
        if (s)
        {
            auto* pool = s->repo->pool;
            // figure out who requires `s`
            Queue solvables;
            queue_init(&solvables);

            pool_whatmatchesdep(pool, SOLVABLE_REQUIRES, s->name, &solvables, -1);

            if (solvables.count != 0)
            {
                Solvable* rs;
                for (int i = 0; i < solvables.count; i++)
                {
                    rs = pool_id2solvable(pool, solvables.elements[i]);
                    auto it = visited.find(rs);
                    if (it == visited.end())
                    {
                        auto dep_id = dep_graph.add_node(PackageInfo(rs));
                        dep_graph.add_edge(parent, dep_id);
                        visited.insert(std::make_pair(rs, dep_id));
                        reverse_walk_graph(dep_graph, dep_id, rs, visited);
                    }
                    else
                    {
                        dep_graph.add_edge(parent, it->second);
                    }
                }
                queue_free(&solvables);
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

    auto print_solvable = [](auto& pkg)
    {
        std::string title = pkg->name + " " + pkg->version + " " + pkg->build_string;
        std::cout << title << std::endl;
        std::cout << std::string(title.size(), '-') << std::endl;
        printf("%-12s: %1s\n", "file name", pkg->fn.c_str());
        printf("%-12s: %1s\n", "name", pkg->name.c_str());
        printf("%-12s: %1s\n", "version", pkg->version.c_str());
        printf("%-12s: %1s\n", "build", pkg->build_string.c_str());
        printf("%-12s: %1lu\n", "build number", pkg->build_number);
        printf("%-12s: %1lu KB\n", "size", pkg->size / 1000);
        printf("%-12s: %1s\n", "license", pkg->license.c_str());
        printf("%-12s: %1s\n", "subdir", pkg->subdir.c_str());
        printf("%-12s: %1s\n", "url", pkg->url.c_str());
        printf("%-12s: %1s\n", "md5", pkg->md5.c_str());
        time_t timestamp = pkg->timestamp / 1000;
        char timestamp_buffer[100];
        strftime(timestamp_buffer,
                 sizeof(timestamp_buffer),
                 "%Y-%m-%d %H:%M:%S",
                 std::gmtime(&timestamp));
        std::string timestamp_string(timestamp_buffer);
        printf("%-12s: %1s UTC\n", "timestamp", timestamp_string.c_str());
        if (!pkg->constrains.empty())
        {
            std::vector<std::string> constrains = pkg->constrains;
            printf("%-12s: \n", "constraints");
            for (auto& each_constraint : constrains)
            {
                printf("  - %1s\n", each_constraint.c_str());
            }
        }
        if (!pkg->depends.empty())
        {
            std::vector<std::string> depends = pkg->depends;
            printf("%-12s: \n", "dependencies");
            for (auto& each_dependency : depends)
            {
                printf("  - %1s\n", each_dependency.c_str());
            }
        }
        std::cout << std::endl;
    };

    query_result Query::find(const std::string& query) const
    {
        Queue job, solvables;
        queue_init(&job);
        queue_init(&solvables);

        Id id = pool_conda_matchspec(m_pool.get(), query.c_str());
        if (id)
        {
            queue_push2(&job, SOLVER_SOLVABLE_PROVIDES, id);
        }
        else
        {
            throw std::runtime_error("Could not generate query for " + query);
        }

        selection_solvables(m_pool.get(), &job, &solvables);
        query_result::dependency_graph g;

        Pool* pool = m_pool.get();
        std::sort(solvables.elements,
                  solvables.elements + solvables.count,
                  [pool](Id a, Id b)
                  {
                      Solvable* sa;
                      Solvable* sb;
                      sa = pool_id2solvable(pool, a);
                      sb = pool_id2solvable(pool, b);
                      return (pool_evrcmp(pool, sa->evr, sb->evr, EVRCMP_COMPARE) > 0);
                  });

        for (int i = 0; i < solvables.count; i++)
        {
            Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
            g.add_node(PackageInfo(s));
        }

        queue_free(&job);
        queue_free(&solvables);

        return query_result(QueryType::kSEARCH, query, std::move(g));
    }

    query_result Query::whoneeds(const std::string& query, bool tree) const
    {
        Queue job, solvables;
        queue_init(&job);
        queue_init(&solvables);

        Id id = pool_conda_matchspec(m_pool.get(), query.c_str());
        if (id)
        {
            queue_push2(&job, SOLVER_SOLVABLE_PROVIDES, id);
        }
        else
        {
            throw std::runtime_error("Could not generate query for " + query);
        }

        query_result::dependency_graph g;

        if (tree)
        {
            selection_solvables(m_pool.get(), &job, &solvables);
            if (solvables.count > 0)
            {
                Solvable* latest = pool_id2solvable(m_pool.get(), solvables.elements[0]);
                auto id = g.add_node(PackageInfo(latest));
                std::map<Solvable*, size_t> visited = { { latest, id } };
                reverse_walk_graph(g, id, latest, visited);
            }
        }
        else
        {
            pool_whatmatchesdep(m_pool.get(), SOLVABLE_REQUIRES, id, &solvables, -1);
            for (int i = 0; i < solvables.count; i++)
            {
                Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
                g.add_node(PackageInfo(s));
            }
        }
        return query_result(QueryType::kWHONEEDS, query, std::move(g));
    }

    query_result Query::depends(const std::string& query, bool tree) const
    {
        Queue job, solvables;
        queue_init(&job);
        queue_init(&solvables);

        Id id = pool_conda_matchspec(m_pool.get(), query.c_str());
        if (id)
        {
            queue_push2(&job, SOLVER_SOLVABLE_PROVIDES, id);
        }
        else
        {
            throw std::runtime_error("Could not generate query for " + query);
        }

        query_result::dependency_graph g;
        selection_solvables(m_pool.get(), &job, &solvables);

        int depth = tree ? -1 : 1;

        auto find_latest = [&](Queue& solvables) -> Solvable*
        {
            Solvable* latest = pool_id2solvable(m_pool.get(), solvables.elements[0]);
            for (int i = 1; i < solvables.count; ++i)
            {
                Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
                if (pool_evrcmp(m_pool.get(), s->evr, latest->evr, 0) > 0)
                {
                    latest = s;
                }
            }
            return latest;
        };

        if (solvables.count > 0)
        {
            Solvable* latest = find_latest(solvables);
            auto id = g.add_node(PackageInfo(latest));
            std::map<Solvable*, size_t> visited = { { latest, id } };
            std::map<std::string, size_t> not_found;
            walk_graph(g, id, latest, visited, not_found, depth);
        }

        queue_free(&job);
        queue_free(&solvables);

        return query_result(QueryType::kDEPENDS, query, std::move(g));
    }

    /*******************************
     * query_result implementation *
     *******************************/

    query_result::query_result(QueryType type,
                               const std::string& query,
                               dependency_graph&& dep_graph)
        : m_type(type)
        , m_query(query)
        , m_dep_graph(std::move(dep_graph))
        , m_pkg_view_list(m_dep_graph.get_node_list().size())
        , m_ordered_pkg_list()
    {
        reset_pkg_view_list();
    }

    query_result::query_result(const query_result& rhs)
        : m_type(rhs.m_type)
        , m_query(rhs.m_query)
        , m_dep_graph(rhs.m_dep_graph)
        , m_pkg_view_list()
        , m_ordered_pkg_list()
    {
        using std::swap;
        auto offset_lbd = [&rhs, this](auto iter) {
            return m_dep_graph.get_node_list().begin()
                   + (iter - rhs.m_dep_graph.get_node_list().begin());
        };

        package_view_list tmp(rhs.m_pkg_view_list.size());
        std::transform(
            rhs.m_pkg_view_list.begin(), rhs.m_pkg_view_list.end(), tmp.begin(), offset_lbd);
        swap(tmp, m_pkg_view_list);

        if (!rhs.m_ordered_pkg_list.empty())
        {
            auto tmp(rhs.m_ordered_pkg_list);
            std::for_each(tmp.begin(),
                          tmp.end(),
                          [offset_lbd](auto& entry) {
                              std::transform(entry.second.begin(),
                                             entry.second.end(),
                                             entry.second.begin(),
                                             offset_lbd);
                          });
            swap(m_ordered_pkg_list, tmp);
        }
    }

    query_result& query_result::operator=(const query_result& rhs)
    {
        if (this != &rhs)
        {
            using std::swap;
            query_result tmp(rhs);
            swap(*this, tmp);
        }
        return *this;
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
        auto fun = PackageInfo::less(field);

        if (!m_ordered_pkg_list.empty())
        {
            for (auto& entry : m_ordered_pkg_list)
            {
                std::sort(entry.second.begin(),
                          entry.second.end(),
                          [fun](const auto& lhs, const auto& rhs) { return fun(*lhs, *rhs); });
            }
        }
        else
        {
            std::sort(m_pkg_view_list.begin(),
                      m_pkg_view_list.end(),
                      [fun](const auto& lhs, const auto& rhs) { return fun(*lhs, *rhs); });
        }

        return *this;
    }

    query_result& query_result::groupby(std::string field)
    {
        auto fun = PackageInfo::get_field_getter(field);
        if (m_ordered_pkg_list.empty())
        {
            for (auto& pkg : m_pkg_view_list)
            {
                m_ordered_pkg_list[fun(*pkg)].push_back(pkg);
            }
        }
        else
        {
            ordered_package_list tmp;
            for (auto& entry : m_ordered_pkg_list)
            {
                for (auto& pkg : entry.second)
                {
                    std::string key = entry.first + '/' + fun(*pkg);
                    tmp[key].push_back(pkg);
                }
            }
            m_ordered_pkg_list = std::move(tmp);
        }
        return *this;
    }

    query_result& query_result::reset()
    {
        reset_pkg_view_list();
        m_ordered_pkg_list.clear();
        return *this;
    }

    std::ostream& query_result::table(std::ostream& out) const
    {
        return table(out, { "Name", "Version", "Build", "Channel" });
    }

    std::ostream& query_result::table(std::ostream& out, const std::vector<std::string>& fmt) const
    {
        if (m_pkg_view_list.empty())
        {
            out << "No entries matching \"" << m_query << "\" found" << std::endl;
        }

        std::vector<mamba::printers::FormattedString> headers;
        std::vector<std::string> cmds, args;
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
                auto sfmt = split(f, ":", 1);
                headers.push_back(sfmt[0]);
                cmds.push_back(sfmt[0]);
                args.push_back(sfmt[1]);
            }
        }

        auto format_row = [&](auto& pkg)
        {
            std::vector<mamba::printers::FormattedString> row;
            for (std::size_t i = 0; i < cmds.size(); ++i)
            {
                const auto& cmd = cmds[i];
                if (cmd == "Name")
                {
                    row.push_back(pkg->name);
                }
                else if (cmd == "Version")
                {
                    row.push_back(pkg->version);
                }
                else if (cmd == "Build")
                {
                    row.push_back(pkg->build_string);
                }
                else if (cmd == "Channel")
                {
                    row.push_back(cut_repo_name(pkg->channel));
                }
                else if (cmd == "Depends")
                {
                    std::string depends_qualifier;
                    for (const auto& dep : pkg->depends)
                    {
                        if (starts_with(dep, args[i]))
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

        if (!m_ordered_pkg_list.empty())
        {
            for (auto& entry : m_ordered_pkg_list)
            {
                for (auto& pkg : entry.second)
                {
                    printer.add_row(format_row(pkg));
                }
            }
        }
        else
        {
            for (const auto& pkg : m_pkg_view_list)
            {
                printer.add_row(format_row(pkg));
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
            m_out << get_package_repr(g.get_node_list()[node]) << '\n';
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

        void finish_node(node_id node, const graph_type&)
        {
            m_prefix_stack.pop_back();
        }

        void start_edge(node_id from, node_id to, const graph_type& g)
        {
            m_is_last = g.get_edge_list(from).back() == to;
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
            m_out << concat("\033[2m", g.get_node_list()[to].name, " already visited", "\033[00m")
                  << '\n';
        }

        void finish_edge(node_id from, node_id to, const graph_type& g)
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
        bool use_graph
            = !m_dep_graph.get_node_list().empty() && !m_dep_graph.get_edge_list(0).empty();
        if (use_graph)
        {
            graph_printer printer(out);
            m_dep_graph.depth_first_search(printer);
        }
        else if (!m_pkg_view_list.empty())
        {
            out << m_query << '\n';
            for (size_t i = 0; i < m_pkg_view_list.size() - 1; ++i)
            {
                out << "  ├─ " << get_package_repr(*m_pkg_view_list[i]) << '\n';
            }
            out << "  └─ " << get_package_repr(*m_pkg_view_list.back()) << '\n';
        }

        return out;
    }

    std::ostream& query_result::pretty(std::ostream& out) const
    {
        if (!m_pkg_view_list.empty())
        {
            for (const auto& pkg : m_pkg_view_list)
            {
                print_solvable(pkg);
            }
        }
        return out;
    }

    nlohmann::json query_result::json() const
    {
        nlohmann::json j;
        std::string query_type = m_type == QueryType::kSEARCH
                                     ? "search"
                                     : (m_type == QueryType::kDEPENDS ? "depends" : "whoneeds");
        j["query"] = { { "query", MatchSpec(m_query).conda_build_form() }, { "type", query_type } };

        std::string msg
            = m_pkg_view_list.empty() ? "No entries matching \"" + m_query + "\" found" : "";
        j["result"] = { { "msg", msg }, { "status", "OK" } };

        j["result"]["pkgs"] = nlohmann::json::array();
        for (size_t i = 0; i < m_pkg_view_list.size(); ++i)
        {
            j["result"]["pkgs"].push_back(m_pkg_view_list[i]->json_record());
        }

        if (m_type != QueryType::kSEARCH && !m_pkg_view_list.empty())
        {
            bool has_root = !m_dep_graph.get_edge_list(0).empty();
            j["result"]["graph_roots"] = nlohmann::json::array();
            j["result"]["graph_roots"].push_back(
                has_root ? m_dep_graph.get_node_list()[0].json_record() : nlohmann::json(m_query));
        }
        return j;
    }

    void query_result::reset_pkg_view_list()
    {
        auto it = m_dep_graph.get_node_list().begin();
        std::generate(m_pkg_view_list.begin(), m_pkg_view_list.end(), [&it]() { return it++; });
    }

    std::string query_result::get_package_repr(const PackageInfo& pkg) const
    {
        return pkg.version.empty() ? pkg.name : pkg.name + '[' + pkg.version + ']';
    }
}  // namespace mamba
