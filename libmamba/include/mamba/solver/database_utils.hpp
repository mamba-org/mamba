#ifndef MAMBA_SOLVER_DATABASE_UTILS_HPP
#define MAMBA_SOLVER_DATABASE_UTILS_HPP

#include <stdexcept>

#include "mamba/solver/database.hpp"

namespace mamba::solver
{
    inline bool database_has_package(DatabaseVariant& database, const specs::MatchSpec& spec)
    {
        if (auto* libsolv_db = std::get_if<libsolv::Database>(&database))
        {
            return libsolv_db->has_package(spec);
        }
        else if (auto* resolvo_db = std::get_if<resolvo::Database>(&database))
        {
            return resolvo_db->has_package(spec);
        }
        throw std::runtime_error("Invalid database variant");
    }
}

#endif  // MAMBA_SOLVER_DATABASE_UTILS_HPP
