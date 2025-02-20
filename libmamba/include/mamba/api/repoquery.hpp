// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iosfwd>
#include <string>
#include <vector>

#include "mamba/api/configuration.hpp"
#include "mamba/core/query.hpp"

namespace mamba
{
    enum class QueryResultFormat
    {
        Json = 0,
        Tree = 1,
        Table = 2,
        Pretty = 3,
        RecursiveTable = 4,
    };

    [[nodiscard]] auto make_repoquery(
        solver::libsolv::Database& database,
        QueryType type,
        QueryResultFormat format,
        const std::vector<std::string>& queries,
        bool show_all_builds,
        const Context::GraphicsParams& graphics_params,
        std::ostream& out
    ) -> bool;

    [[nodiscard]] auto repoquery(
        Configuration& config,
        QueryType type,
        QueryResultFormat format,
        bool use_local,
        const std::vector<std::string>& query
    ) -> bool;
}
