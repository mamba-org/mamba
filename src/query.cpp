#include <sstream>

#include "query.hpp"
#include "util.hpp"

#include "tabulate/table.hpp"

namespace mamba
{
    std::string cut_repo_name(std::ostream& out, const std::string_view& reponame)
    {
        if (starts_with(reponame, "https://conda.anaconda.org/"))
        {
            return reponame.substr(27, std::string::npos).data();
        }
        if (starts_with(reponame, "https://repo.anaconda.com/"))
        {
            return reponame.substr(26, std::string::npos).data();
        }
        return reponame.data();
    }

    void solvable_to_stream(std::ostream& out, Solvable* s, int row_count,
        tabulate::Table& query_result)
    {
        Pool* pool = s->repo->pool;

        std::string channel = cut_repo_name(out, s->repo->name);
        std::string name = pool_id2str(pool, s->name);
        std::string evr = pool_id2str(pool, s->evr); 
        std::string build_flavor = solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR);

        query_result.add_row({name, evr, build_flavor, channel});
        query_result[row_count].format().border_top(" ").border_bottom(" ");
    }

    void print_dep_graph(std::ostream& out, Solvable* s, int level, int max_level)
    {
       if (level <= max_level) {
         auto* pool = s->repo->pool;
         for(int i = 0; i< level; i++)
           out << "\x74 ";
          out << pool_id2str(pool, s->name) << "-v-" << pool_id2str(pool, s->evr)<< std::endl;

          if (s->requires)
          {
            Id *reqp = s->repo->idarraydata + s->requires;
            Id req = *reqp;
            while (req != 0)            /* go through all requires */
            {
              Queue job, rec_solvables;
              queue_init(&rec_solvables);
              queue_init(&job);

              queue_push2(&job, SOLVER_SOLVABLE_PROVIDES, req);
              selection_solvables(pool, &job, &rec_solvables);
              int index = 0;
              for (int i = 0; i < rec_solvables.count; i++)
              {
                Solvable* s = pool_id2solvable(pool, rec_solvables.elements[i]);
                if (s->name == req){
                    index = i;
                }
              }
              Solvable *sreq = pool_id2solvable(pool, rec_solvables.elements[index]);
              print_dep_graph(out, sreq, level + 1, max_level);

              queue_free(&rec_solvables);
              ++reqp;
              req = *reqp;
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

        tabulate::Table find_table_results;
        find_table_results.add_row({"Name", "Version", "Build", "Channel"});
        for (int i = 0; i < solvables.count; i++)
        {
            Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
            solvable_to_stream(out, s, i + 1, find_table_results);
        }

        find_table_results[0].format()
            .border_top("-")
            .font_style({tabulate::FontStyle::bold});
        find_table_results[1].format().border_top("-");
        find_table_results[solvables.count].format().border_bottom("-");
        out << find_table_results << std::endl;

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
        tabulate::Table whatrequires_table_results;
        whatrequires_table_results.add_row({"Name", "Version", "Build", "Channel"});
        for (int i = 0; i < solvables.count; i++)
        {
            Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[i]);
            solvable_to_stream(out, s, i + 1, whatrequires_table_results);
        }
        whatrequires_table_results[0].format()
            .border_top("-")
            .font_style({tabulate::FontStyle::bold});
        whatrequires_table_results[1].format().border_top("-");
        whatrequires_table_results[solvables.count].format().border_bottom("-");
        out << whatrequires_table_results << std::endl;

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

        std::stringstream out;
        if (solvables.count == 0)
        {
            out << "No entries matching \"" << query << "\" found";
        }

        // Print conda-tree like dependency graph for upto a recursion limit
        int dependency_recursion_limit = 3;
        if (solvables.count > 0)
        {
           Solvable* s = pool_id2solvable(m_pool.get(), solvables.elements[0]);
           print_dep_graph(out, s, 0, dependency_recursion_limit);
        }

        out << std::endl;

        queue_free(&job);
        queue_free(&solvables);

        return out.str();
    }

}
