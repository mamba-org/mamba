#include <sstream>
#include <iomanip>
#include <numeric>
#include <set>

#include "query.hpp"
#include "match_spec.hpp"
#include "package_info.hpp"
#include "output.hpp"
#include "util.hpp"

extern "C" {
    #include <solv/evr.h>
}

namespace mamba
{
    void walk_graph(QueryResult::package_list& pkg_list,
                    QueryResult::package_tree& parent,
                    Solvable* s,
                    std::set<Solvable*>& visited_solvs)
    {
        if (s && s->requires)
        {
            auto* pool = s->repo->pool;

            Id* reqp = s->repo->idarraydata + s->requires;
            Id req = *reqp;
            bool already_visited = false;
            auto already_visited_iter = pkg_list.end();
            bool not_found = false;
            auto not_found_iter = pkg_list.end();

            while (req != 0) /* go through all requires */
            {
                Queue job, rec_solvables;
                queue_init(&rec_solvables);
                queue_init(&job);
                // the following prints the requested version
                queue_push2(&job, SOLVER_SOLVABLE_PROVIDES, req);
                selection_solvables(pool, &job, &rec_solvables);

                if (rec_solvables.count != 0)
                {
                    Solvable* rs;
                    for (int i = 0; i < rec_solvables.count; i++)
                    {
                        rs = pool_id2solvable(pool, rec_solvables.elements[i]);
                        if (rs->name == req)
                        {
                            break;
                        }
                    }
                    if (visited_solvs.count(rs) == 0)
                    {
                        pkg_list.push_back(PackageInfo(rs));
                        QueryResult::package_tree next_node(--pkg_list.end());
                        visited_solvs.insert(rs);
                        walk_graph(pkg_list, next_node, rs, visited_solvs);
                        parent.add_child(std::move(next_node));
                    }
                    else
                    {
                        if (!already_visited)
                        {
                            pkg_list.push_back(PackageInfo(concat("\033[2m", pool_id2str(pool, rs->name), " already visited", "\033[00m")));
                            already_visited_iter = --pkg_list.end();
                            already_visited = true;
                        }
                        parent.add_child(already_visited_iter);
                    }
                }
                else
                {
                    if (!not_found)
                    {
                        pkg_list.push_back(PackageInfo(concat(pool_id2str(pool, req), " >>> NOT FOUND <<<")));
                        not_found_iter = --pkg_list.end();
                        not_found = true;
                    }
                    parent.add_child(not_found_iter);
                }
                queue_free(&rec_solvables);
                ++reqp;
                req = *reqp;
            }
        }
    }

    void reverse_walk_graph(QueryResult::package_list& pkg_list,
                            QueryResult::package_tree& parent,
                            Solvable* s,
                            std::set<Solvable*>& visited_solvs)
    {
        if (s)
        {
            auto* pool = s->repo->pool;
            // figure out who requires `s`
            Queue solvables;
            queue_init(&solvables);

            pool_whatmatchesdep(pool, SOLVABLE_REQUIRES, s->name, &solvables, -1);
            bool already_visited = false;
            auto already_visited_iter = pkg_list.end();

            if (solvables.count != 0)
            {
                Solvable* rs;
                for (int i = 0; i < solvables.count; i++)
                {
                    rs = pool_id2solvable(pool, solvables.elements[i]);
                    if (visited_solvs.count(rs) == 0)
                    {
                        pkg_list.push_back(PackageInfo(rs));
                        QueryResult::package_tree next_node(--pkg_list.end());
                        visited_solvs.insert(rs);
                        reverse_walk_graph(pkg_list, next_node, rs, visited_solvs);
                        parent.add_child(std::move(next_node));
                    }
                    else
                    {
                        if (!already_visited)
                        {
                            pkg_list.push_back(PackageInfo(concat("\033[2m", pool_id2str(pool, rs->name), " already visited", "\033[00m")));
                            already_visited_iter = --pkg_list.end();
                            already_visited = true;
                        }
                        parent.add_child(already_visited_iter);
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

    QueryResult Query::find(const std::string& query) const
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
        QueryResult::package_list pkg_list;

        Pool* pool = m_pool.get();
        std::sort(solvables.elements, solvables.elements + solvables.count, [pool](Id a, Id b) {
            Solvable* sa; Solvable* sb;
            sa = pool_id2solvable(pool, a);
            sb = pool_id2solvable(pool, b);
            return (pool_evrcmp(pool, sa->evr, sb->evr, EVRCMP_COMPARE) > 0); 
        });
        
        for (int i = 0; i < solvables.count; i++)
        {
            Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
            pkg_list.push_back(PackageInfo(s));
        }

        queue_free(&job);
        queue_free(&solvables);

        return QueryResult(QueryType::Search, query, std::move(pkg_list));
    }

    QueryResult Query::whoneeds(const std::string& query, bool tree) const
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

        QueryResult::package_list pkg_list;
        QueryResult::package_tree_ptr root = nullptr;

        if (tree)
        {
            selection_solvables(m_pool.get(), &job, &solvables);
            if (solvables.count > 0)
            {
                Solvable* latest = pool_id2solvable(m_pool.get(), solvables.elements[0]);
                pkg_list.push_back(PackageInfo(latest));
                root.reset(new QueryResult::package_tree(--pkg_list.end()));
                std::set<Solvable*> visited { latest };
                reverse_walk_graph(pkg_list, *root, latest, visited);
            }
        }
        else
        {
            pool_whatmatchesdep(m_pool.get(), SOLVABLE_REQUIRES, id, &solvables, -1);
            for (int i = 0; i < solvables.count; i++)
            {
                Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
                pkg_list.push_back(PackageInfo(s));
            }
        }
        return QueryResult(QueryType::Whoneeds,
                           query,
                           std::move(pkg_list),
                           std::move(root));
    }

    QueryResult Query::depends(const std::string& query) const
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

        QueryResult::package_list pkg_list;
        QueryResult::package_tree_ptr root = nullptr;
        selection_solvables(m_pool.get(), &job, &solvables);

        if (solvables.count > 0)
        {
            Solvable* latest = pool_id2solvable(m_pool.get(), solvables.elements[0]);
            for (int i = 1; i < solvables.count; ++i)
            {
                Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
                if (pool_evrcmp_str(m_pool.get(),
                                    pool_id2evr(m_pool.get(), s->evr),
                                    pool_id2evr(m_pool.get(), latest->evr), 0) > 0)
                {
                    latest = s;
                }
            }

            pkg_list.push_back(PackageInfo(latest));
            root.reset(new QueryResult::package_tree(--pkg_list.end()));
            std::set<Solvable*> visited { latest };
            walk_graph(pkg_list, *root, latest, visited);
        }

        queue_free(&job);
        queue_free(&solvables);

        return QueryResult(QueryType::Depends,
                           query,
                           std::move(pkg_list),
                           std::move(root));
    }


    /******************************
     * QueryResult implementation *
     ******************************/

    QueryResult::QueryResult(QueryType type,
                             const std::string& query,
                             package_list&& pkg_list)
        : QueryResult(type, query, std::move(pkg_list), nullptr)
    {
    }

    QueryResult::QueryResult(QueryType type,
                             const std::string& query,
                             package_list&& pkg_list,
                             package_tree_ptr pkg_tree)
        : m_type(type)
        , m_query(query)
        , m_pkg_list(std::move(pkg_list))
        , m_pkg_view_list(m_pkg_list.size())
        , p_pkg_tree(std::move(pkg_tree))
        , m_ordered_pkg_list()
    {
        reset_pkg_view_list();
    }

    QueryResult::QueryResult(const QueryResult& rhs)
        : m_type(rhs.m_type)
        , m_query(rhs.m_query)
        , m_pkg_list(rhs.m_pkg_list)
        , m_pkg_view_list()
        , p_pkg_tree(nullptr)
        , m_ordered_pkg_list()
    {
        using std::swap;
        auto offset_lbd = [&rhs, this](auto iter)
        {
            return m_pkg_list.begin() + (iter - rhs.m_pkg_list.begin());
        };

        package_view_list tmp(rhs.m_pkg_view_list.size());
        std::transform
        (
            rhs.m_pkg_view_list.begin(),
            rhs.m_pkg_view_list.end(),
            tmp.begin(),
            offset_lbd
        );
        swap(tmp, m_pkg_view_list);

        if (rhs.p_pkg_tree)
        {
            package_tree_ptr tmp(new tree_node(*rhs.p_pkg_tree));
            update_pkg_node(*tmp, rhs.m_pkg_list);
            p_pkg_tree = std::move(tmp);
        }

        if (!rhs.m_ordered_pkg_list.empty())
        {
            auto tmp(rhs.m_ordered_pkg_list);
            std::for_each
            (
                tmp.begin(),
                tmp.end(),
                [offset_lbd](auto& entry)
                {
                    std::transform
                    (
                        entry.second.begin(),
                        entry.second.end(),
                        entry.second.begin(),
                        offset_lbd
                    );
                }
            );
            swap(m_ordered_pkg_list, tmp);
        }
    }

    QueryResult& QueryResult::operator=(const QueryResult& rhs)
    {
        if (this != &rhs)
        {
            using std::swap;
            QueryResult tmp(rhs);
            swap(*this, tmp);
        }
        return *this;
    }

    QueryType QueryResult::query_type() const
    {
        return m_type;
    }

    const std::string& QueryResult::query() const
    {
        return m_query;
    }

    QueryResult& QueryResult::sort(std::string field)
    {
        auto fun = PackageInfo::less(field);

        if (!m_ordered_pkg_list.empty())
        {
            for (auto& entry: m_ordered_pkg_list)
            {
                std::sort(entry.second.begin(), entry.second.end(),
                    [fun](const auto& lhs, const auto& rhs) { return fun(*lhs, *rhs); });
            }
        }
        else
        {
            std::sort(m_pkg_view_list.begin(), m_pkg_view_list.end(),
                [fun](const auto& lhs, const auto& rhs) { return fun(*lhs, *rhs); });
        }
        if (p_pkg_tree)
        {
            sort_tree_node(*p_pkg_tree, fun);
        }
        return *this;
    }

    QueryResult& QueryResult::groupby(std::string field)
    {
        auto fun = PackageInfo::get_field_getter(field);
        if (m_ordered_pkg_list.empty())
        {
            for (auto& pkg: m_pkg_view_list)
            {
                m_ordered_pkg_list[fun(*pkg)].push_back(pkg);
            }
        }
        else
        {
            ordered_package_list tmp;
            for (auto& entry: m_ordered_pkg_list)
            {
                for (auto& pkg: entry.second)
                {
                    std::string key = entry.first + '/' + fun(*pkg);
                    tmp[key].push_back(pkg);
                }
            }
            m_ordered_pkg_list = std::move(tmp);
        }
        return *this;
    }

    QueryResult& QueryResult::reset()
    {
        reset_pkg_view_list();
        m_ordered_pkg_list.clear();
        return *this;
    }
    
    std::ostream& QueryResult::table(std::ostream& out) const
    {
        if (m_pkg_view_list.empty())
        {
            out << "No entries matching \"" << m_query << "\" found";
        }

        printers::Table printer({"Name", "Version", "Build", "Channel"});

        if (!m_ordered_pkg_list.empty())
        {
            for (auto& entry: m_ordered_pkg_list)
            {
                for (auto& pkg: entry.second)
                {
                    printer.add_row({pkg->name, pkg->version, pkg->build_string, cut_repo_name(pkg->channel)});
                }
            }
        }
        else
        {
            for (const auto& pkg: m_pkg_view_list)
            {
                printer.add_row({pkg->name, pkg->version, pkg->build_string, pkg->channel});
            }
        }
        return printer.print(out);
    }

    std::ostream& QueryResult::tree(std::ostream& out) const
    {
        if (p_pkg_tree)
        {
            print_tree_node(out, *p_pkg_tree, "", false, true);
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

    nl::json QueryResult::json() const
    {
        nl::json j;
        std::string query_type = m_type == QueryType::Search 
                               ? "search"
                               : (m_type == QueryType::Depends
                               ? "depends"
                               : "whoneeds");
        j["query"] = {
            {"query", MatchSpec(m_query).conda_build_form()},
            {"type", query_type}
        };

        std::string msg = m_pkg_view_list.empty() ? "No entries matching \"" + m_query + "\" found" : "";
        j["result"] = {
            {"msg", msg},
            {"status", "OK"}
        };

        j["result"]["pkgs"] = nlohmann::json::array();
        for (size_t i = 0; i < m_pkg_view_list.size(); ++i)
        {
            j["result"]["pkgs"].push_back(m_pkg_view_list[i]->json());
        }

        if (m_type != QueryType::Search)
        {
            j["result"]["graph_roots"] = nlohmann::json::array();
            j["result"]["graph_roots"].push_back(p_pkg_tree ? p_pkg_tree->m_value->json() : nl::json(m_query));
        }
        return j;
    }

    void QueryResult::update_pkg_node(package_tree& node, const package_list& src)
    {
         node.m_value = m_pkg_list.begin() + (node.m_value - src.begin());
         for (auto& child: node.m_children)
         {
             update_pkg_node(child, src);
         }
    }
    
    void QueryResult::reset_pkg_view_list()
    {
        auto it = m_pkg_list.begin();
        std::generate(m_pkg_view_list.begin(),
                      m_pkg_view_list.end(),
                      [&it]() { return it++; });

    }

    std::string QueryResult::get_package_repr(const PackageInfo& pkg) const
    {
        return pkg.name + '[' + pkg.version + '[';
    }

    void QueryResult::print_tree_node(std::ostream& out,
                                      const package_tree& node,
                                      const std::string& prefix,
                                      bool is_last,
                                      bool is_root) const
    {
        out << prefix;
        if (!is_root)
        {
            out << (is_last ? "└─ " : "├─ " );
        }
        out << get_package_repr(*(node.m_value)) << '\n';
        std::size_t size = node.m_children.size();
        for (std::size_t i = 0; i < size; ++i)
        {
            std::string next_prefix = prefix + (is_last || is_root ? "  " : "│ ");
            print_tree_node(out, node.m_children[i], next_prefix, i == size - 1, false);
        }
    }

    void QueryResult::sort_tree_node(package_tree& node,
                                     const PackageInfo::compare_fun& fun)
    {
        std::sort(node.m_children.begin(), node.m_children.end(),
            [&fun](const package_tree& lhs, const package_tree& rhs) { return fun(*(lhs.m_value), *(rhs.m_value)); });
        for (auto& ch: node.m_children)
        {
            sort_tree_node(ch, fun);
        }
    }
}
