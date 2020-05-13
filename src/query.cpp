#include <sstream>
#include <iomanip>
#include <numeric>
#include <set>

#include "query.hpp"
#include "output.hpp"
#include "util.hpp"

extern "C" {
    #include <solv/evr.h>
}

#include "tabulate/table.hpp"

namespace mamba
{
    namespace printers
    {
        template <class V>
        class Node
        {
        public:
            Node(const V& s)
                : m_self(s)
            {
            }

            void set_root(bool r)
            {
                m_root = r;
            }

            V m_self;
            std::vector<Node<V>> m_children;
            bool m_root = false;

            void add_child(const V& n) 
            {
                m_children.push_back(Node<V>(n));
            }

            void add_child(const Node<V>& n) 
            {
                m_children.push_back(n);
            }

            void print(const std::string& prefix, bool is_last)
            {
                std::cout << prefix;
                if (!m_root)
                {
                    std::cout << (is_last ? "└─ " : "├─ ");
                }
                std::cout << m_self << "\n";
                for (std::size_t i = 0; i < m_children.size(); ++i)
                {
                    std::string next_prefix = prefix;
                    next_prefix += (is_last || m_root) ? "  " : "│ ";
                    m_children[i].print(next_prefix, i == m_children.size() - 1);
                }
            }
        };
    }

    void solvable_to_stream(std::ostream& out, Solvable* s, int row_count, printers::Table& query_result)
    {
        Pool* pool = s->repo->pool;

        std::string channel = cut_repo_name(s->repo->name);
        std::string name = pool_id2str(pool, s->name);
        std::string evr = pool_id2str(pool, s->evr); 
        std::string build_flavor = solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR);

        query_result.add_row({name, evr, build_flavor, channel});
    }

    void walk_graph(printers::Node<std::string>& parent, 
                    Solvable* s,
                    std::set<Solvable*>& visited_solvs)
    {
        if (s && s->requires)
        {
            auto* pool = s->repo->pool;

            Id* reqp = s->repo->idarraydata + s->requires;
            Id req = *reqp;
            while (req != 0) /* go through all requires */
            {
                Queue job, rec_solvables;
                queue_init(&rec_solvables);
                queue_init(&job);
                // the following prints the requested version
                queue_push2(&job, SOLVER_SOLVABLE_PROVIDES, req);
                selection_solvables(pool, &job, &rec_solvables);
                int index = 0;

                bool next_is_last = *(reqp + 1) == 0;

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
                        printers::Node<std::string> next_node(concat(pool_id2str(pool, req), " [", pool_id2evr(pool, req), "]"));
                        visited_solvs.insert(rs);
                        walk_graph(next_node, rs, visited_solvs);
                        parent.add_child(next_node);
                    }
                    else
                    {
                        parent.add_child(concat("\033[2m", pool_id2str(pool, rs->name), " already visited", "\033[00m"));
                    }
                }
                else
                {
                    parent.add_child(concat(pool_id2str(pool, req), " >>> NOT FOUND <<<"));
                    // print_dep_graph(out, nullptr, ">>> NOT FOUND <<< " + solv_str.str(), level + 1, max_level,
                    //                 next_is_last, next_prefix);
                }
                queue_free(&rec_solvables);
                ++reqp;
                req = *reqp;
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

    std::string Query::find(const std::string& query)
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

        std::stringstream out;
        if (solvables.count == 0)
        {
            out << "No entries matching \"" << query << "\" found";
            return out.str();
        }

        printers::Table find_table_results({"Name", "Version", "Build", "Channel"});
        for (int i = 0; i < solvables.count; i++)
        {
            Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
            solvable_to_stream(out, s, i + 1, find_table_results);
        }

        find_table_results.print();

        queue_free(&job);
        queue_free(&solvables);

        return out.str();
    }

    std::string Query::whatrequires(const std::string& query)
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

        pool_whatmatchesdep(m_pool.get(), SOLVABLE_REQUIRES, id, &solvables, -1);

        std::stringstream out;
        if (solvables.count == 0)
        {
            out << "No entries matching \"" << query << "\" found";
        }
        
        printers::Table whatrequires_table_results({"Name", "Version", "Build", "Channel"});
        for (int i = 0; i < solvables.count; i++)
        {
            Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
            solvable_to_stream(out, s, i + 1, whatrequires_table_results);
        }
        whatrequires_table_results.print();

        queue_free(&job);
        queue_free(&solvables);

        return out.str();
    }

   std::string Query::dependencytree(const std::string& query)
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

        // Print conda-tree like dependency graph for upto a recursion limit
        std::stringstream out;
        if (solvables.count == 0)
        {
            out << "No entries matching \"" << query << "\" found";
        }
        else
        {

            Solvable* latest = pool_id2solvable(m_pool.get(), solvables.elements[0]);
            if (solvables.count > 1)
            {
                out << "Found " << solvables.count << " packages for " << query << ", showing only the latest.\n\n";
                for (std::size_t i = 1; i < solvables.count; ++i)
                {
                    Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
                    if (pool_evrcmp_str(m_pool.get(),
                                        pool_id2evr(m_pool.get(), s->evr),
                                        pool_id2evr(m_pool.get(), latest->evr), 0) > 0)
                    {
                        latest = s;
                    }
                }
            }
            printers::Node<std::string> root(concat(pool_id2str(m_pool.get(), latest->name), " == ", pool_id2str(m_pool.get(), latest->evr)));
            root.set_root(true);
            // std::stringstream solv_str;
            // solv_str << pool_id2str(m_pool.get(), latest->name) << " == " << ;
            std::set<Solvable*> visited { latest };
            walk_graph(root, latest, visited);
            root.print("", false);
        }

        queue_free(&job);
        queue_free(&solvables);

        return out.str();
    }

}
