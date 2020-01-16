#ifndef MAMBA_SOLVER_HPP
#define MAMBA_SOLVER_HPP

#include <vector>
#include <string>
#include <stdexcept>


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

class mamba_error
    : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};
 
std::tuple<std::vector<std::tuple<std::string, std::string, std::string>>,
           std::vector<std::tuple<std::string, std::string>>>
solve(std::vector<std::tuple<std::string, std::string, int, int>> repos,
           std::string installed,
           std::vector<std::string> jobs,
           std::vector<std::pair<int, int>> solver_options,
           int solver_flags,
           bool strict_priority,
           bool quiet,
           int debug_level,
           const std::string& mamba_version);

#endif