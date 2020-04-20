#ifndef MAMBA_QUERY_HPP
#define MAMBA_QUERY_HPP

#include <functional>
#include <string_view>

#include "pool.hpp"

extern "C"
{
    #include "solv/conda.h"
    #include "solv/solver.h"
    #include "solv/selection.h"
}

namespace mamba
{
    void cut_repo_name(std::ostream& out, const std::string_view& reponame);
    void solvable_to_stream(std::ostream& out, Solvable* s);

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
