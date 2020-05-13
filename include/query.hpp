#ifndef MAMBA_QUERY_HPP
#define MAMBA_QUERY_HPP

#include <functional>
#include <string_view>

#include "output.hpp"
#include "pool.hpp"

extern "C"
{
    #include "solv/repo.h"
    #include "solv/conda.h"
    #include "solv/solver.h"
    #include "solv/selection.h"
}

namespace mamba
{
    std::string cut_repo_name(std::ostream& out, const std::string_view& reponame);
    void print_dep_graph(std::ostream& out, Solvable* s, const std::string& solv_str, int level, int max_level, bool last, const std::string& prefix);

    class Query
    {
    public:

        Query(MPool& pool);

        std::string find(const std::string& query);
        std::string whatrequires(const std::string& query, bool tree);
        std::string dependencytree(const std::string& query);

    private:

        std::reference_wrapper<MPool> m_pool;
    };
}

#endif // MAMBA_QUERY_HPP
