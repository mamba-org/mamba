#ifndef MAMBA_SOLVER_HPP
#define MAMBA_SOLVER_HPP

#include <vector>
#include <string>

std::tuple<std::vector<std::tuple<std::string, std::string, std::string>>,
           std::vector<std::tuple<std::string, std::string>>>
solve(std::vector<std::tuple<std::string, std::string, int>> repos,
           std::string installed,
           std::vector<std::string> jobs,
           bool update,
           bool strict_priority);

#endif