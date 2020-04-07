#include "pool.hpp"

extern "C"
{
    #include "solv/solver.h"
    #include "solv/selection.h"
}


namespace mamba
{
    class Query
    {
    public:

        Query(MPool& pool)
            : m_pool(pool)
        {
            m_pool.get().create_whatprovides();
        }

        std::string find(const std::string& query)
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

            for (int i = 0; i < solvables.count; i++)
            {
                Id sid = solvables.elements[i];
                Solvable* s = pool_id2solvable(m_pool.get(), sid);
                printf("%s, %s, %s\n", pool_id2str(m_pool.get(), s->name), pool_id2str(m_pool.get(), s->evr), solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR)); //, pool_id2str(pool, s->repo->name));
            }

            queue_free(&job);
            queue_free(&solvables);

            return "worked.";
        }

        std::string whatrequires(const std::string& query)
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

            for (int i = 0; i < solvables.count; i++)
            {
                Id sid = solvables.elements[i];
                Solvable* s = pool_id2solvable(m_pool.get(), sid);
                printf("%s, %s, %s\n", pool_id2str(m_pool.get(), s->name), pool_id2str(m_pool.get(), s->evr), solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR)); //, pool_id2str(pool, s->repo->name));
            }

            queue_free(&job);
            queue_free(&solvables);

            return "done.";
        }

        std::reference_wrapper<MPool> m_pool;
    };
}