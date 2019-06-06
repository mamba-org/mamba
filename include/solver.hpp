#ifndef MAMBA_SOLVER_HPP
#define MAMBA_SOLVER_HPP

#include <vector>
#include <string>

extern "C"
{
    #include "solv/pool.h"
    #include "solv/repo.h"
    #include "solv/queue.h"
    #include "solv/solver.h"
    #include "solv/solverdebug.h"

    #include "solv/conda.h"
    #include "solv/repo_conda.h"
}

std::tuple<std::vector<std::tuple<std::string, std::string, std::string>>,
           std::vector<std::tuple<std::string, std::string>>>
solve(std::vector<std::tuple<std::string, std::string, int>> repos,
           std::string installed,
           std::vector<std::string> jobs,
           int solver_flags,
           bool strict_priority);

#endif