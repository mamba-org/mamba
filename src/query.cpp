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
                }
                queue_free(&rec_solvables);
                ++reqp;
                req = *reqp;
            }
        }
    }

    void reverse_walk_graph(printers::Node<std::string>& parent, 
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

            if (solvables.count != 0)
            {
                Solvable* rs;
                for (int i = 0; i < solvables.count; i++)
                {
                    rs = pool_id2solvable(pool, solvables.elements[i]);
                    if (visited_solvs.count(rs) == 0)
                    {
                        printers::Node<std::string> next_node(concat(pool_id2str(pool, rs->name), " [", pool_id2str(pool, rs->evr), "]"));
                        visited_solvs.insert(rs);
                        reverse_walk_graph(next_node, rs, visited_solvs);
                        parent.add_child(next_node);
                    }
                    else
                    {
                        parent.add_child(concat("\033[2m", pool_id2str(pool, rs->name), " already visited", "\033[00m"));
                    }
                }
                queue_free(&solvables);
            }
            return;
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

        nlohmann::json j;
        j["query"] = {
            {"query", MatchSpec(query).conda_build_form()},
            {"type", "search"}
        };
        j["result"] = {
            {"msg", ""},
            {"status", "OK"}
        };

        j["result"]["pkgs"] = nlohmann::json::array();

        if (Context::instance().json)
        {
            j["result"] = {
                {"msg", ""},
                {"status", "OK"}
            };

            if (solvables.count == 0)
            {
                j["result"]["msg"] = std::string("No entries matching \"") + query + "\" found";
            }

            for (int i = 0; i < solvables.count; i++)
            {
                Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
                j["result"]["pkgs"].push_back(PackageInfo(s).json());
            }
            std::cout << j.dump(4);
        }
        else
        {
            if (solvables.count == 0)
            {
                out << "No entries matching \"" << query << "\" found";
                std::cout << out.str() << std::endl;
            }

            Pool* pool = m_pool.get();
            std::sort(solvables.elements, solvables.elements + solvables.count, [pool](Id a, Id b) {
                Solvable* sa; Solvable* sb;
                sa = pool_id2solvable(pool, a);
                sb = pool_id2solvable(pool, b);
                return (pool_evrcmp(pool, sa->evr, sb->evr, EVRCMP_COMPARE) > 0); 
            });
            // solv_sort(solvables.elements, solvables.count, sizeof(Id), prune_to_best_version_sortcmp, pool);

            printers::Table find_table_results({"Name", "Version", "Build", "Channel"});
            for (int i = 0; i < solvables.count; i++)
            {
                Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
                solvable_to_stream(out, s, i + 1, find_table_results);
            }
            find_table_results.print();
        }

        queue_free(&job);
        queue_free(&solvables);
        return "";
    }

    std::string Query::whoneeds(const std::string& query, bool tree)
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

        if (tree || Context::instance().json)
        {
            std::stringstream msg;
            selection_solvables(m_pool.get(), &job, &solvables);

            nlohmann::json j;
            j["query"] = {
                {"query", MatchSpec(query).conda_build_form()},
                {"type", "whoneeds"}
            };

            j["result"]["pkgs"] = nlohmann::json::array();

            if (solvables.count)
            {
                msg << "Found " << solvables.count << " reverse dependencies";
                Solvable* latest = pool_id2solvable(m_pool.get(), solvables.elements[0]);
                printers::Node<std::string> root(concat(pool_id2str(m_pool.get(), latest->name), " == ", pool_id2str(m_pool.get(), latest->evr)));
                root.set_root(true);
                std::set<Solvable*> visited { latest };

                reverse_walk_graph(root, latest, visited);
                if (Context::instance().json)
                {
                    j["result"] = {
                        {"msg", msg.str()},
                        {"status", "OK"}
                    };
                    auto j_pkgs = nlohmann::json::array();
                    for (auto* s : visited)
                    {
                        j_pkgs.push_back(PackageInfo(s).json());
                    }
                    j["result"]["pkgs"] = j_pkgs;
                    j["result"]["graph_roots"] = nlohmann::json::array();
                    j["result"]["graph_roots"].push_back(std::string(pool_id2str(m_pool.get(), latest->name)));

                    std::cout << j.dump(4) << std::endl;
                }
                else
                {
                    std::cout << msg.str() << std::endl;
                    root.print("", false);
                }
            }
            else
            {
                msg << "No matching package found";

                j["result"]["msg"] = msg.str();
                j["result"]["status"] = "OK";
                std::cout << j.dump(4) << std::endl;
            }
        }
        else
        {
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

        }
        queue_free(&job);
        queue_free(&solvables);

        return "";
        // return out.str();
    }

   std::string Query::depends(const std::string& query)
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

        nlohmann::json j;
        j["query"] = {
            {"query", MatchSpec(query).conda_build_form()},
            {"type", "depends"}
        };
        j["result"] = {
            {"msg", ""}
        };

        std::stringstream out, msg;
        if (solvables.count == 0)
        {
            msg << "No entries matching \"" << query << "\" found";
            j["result"]["msg"] = msg.str();
            j["result"]["status"] = "OK";
        }
        else
        {
            Solvable* latest = pool_id2solvable(m_pool.get(), solvables.elements[0]);
            if (solvables.count == 1)
            {
                msg << "Found " << solvables.count << " package for " << query;
            }
            if (solvables.count > 1)
            {
                msg << "Found " << solvables.count << " packages for " << query << ", showing only the latest.";

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

            j["result"]["msg"] = msg.str();
            j["result"]["status"] = "OK";

            printers::Node<std::string> root(concat(pool_id2str(m_pool.get(), latest->name), " == ", pool_id2str(m_pool.get(), latest->evr)));
            root.set_root(true);
            std::set<Solvable*> visited { latest };
            walk_graph(root, latest, visited);
            auto j_pkgs = nlohmann::json::array();
            for (auto* s : visited)
            {
                j_pkgs.push_back(PackageInfo(s).json());
            }
            j["result"]["pkgs"] = j_pkgs;
            j["result"]["graph_roots"] = nlohmann::json::array();
            j["result"]["graph_roots"].push_back(std::string(pool_id2str(m_pool.get(), latest->name)));

            if (!Context::instance().json)
            {
                std::cout << msg.str() << "\n\n";
                root.print("", false);
            }
        }

        if (Context::instance().json)
        {
            std::cout << j.dump(4) << std::endl;
        }
        else
        {
            std::cout << msg.str() << std::endl;
        }

        queue_free(&job);
        queue_free(&solvables);

        return "";
    }

}
