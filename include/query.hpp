#ifndef MAMBA_QUERY_HPP
#define MAMBA_QUERY_HPP

#include <functional>
#include <string_view>

#include "pool.hpp"

namespace tabulate {
    class Table;
}

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
    void solvable_to_stream(std::ostream& out, Solvable* s, int row_count,
        tabulate::Table& query_result);

    class Query
    {
    public:

        Query(MPool& pool);

        std::string find(const std::string& query);
        std::string whatrequires(const std::string& query);

    private:

        std::reference_wrapper<MPool> m_pool;
    };
}

#endif // MAMBA_QUERY_HPP
