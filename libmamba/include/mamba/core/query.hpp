// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_QUERY_HPP
#define MAMBA_CORE_QUERY_HPP

#include <iosfwd>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "mamba/core/context.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/graph.hpp"

namespace mamba
{
    namespace solver::libsolv
    {
        class Database;
    }

    enum class QueryType
    {
        Search,
        Depends,
        WhoNeeds
    };

    constexpr auto enum_name(QueryType t) -> std::string_view;

    auto query_type_parse(std::string_view name) -> QueryType;

    class QueryResult
    {
    public:

        using GraphicsParams = Context::GraphicsParams;
        using dependency_graph = util::DiGraph<specs::PackageInfo>;

        QueryResult(QueryType type, std::string query, dependency_graph dep_graph);
        QueryResult(const QueryResult&) = default;
        QueryResult(QueryResult&&) = default;

        ~QueryResult() = default;

        auto operator=(const QueryResult&) -> QueryResult& = default;
        auto operator=(QueryResult&&) -> QueryResult& = default;

        [[nodiscard]] auto type() const -> QueryType;
        [[nodiscard]] auto query() const -> const std::string&;
        [[nodiscard]] auto empty() const -> bool;

        auto sort(std::string_view field) -> QueryResult&;

        auto groupby(std::string_view field) -> QueryResult&;

        auto reset() -> QueryResult&;

        auto table(std::ostream&) const -> std::ostream&;
        auto table(std::ostream&, const std::vector<std::string_view>& fmt) const -> std::ostream&;
        [[nodiscard]] auto table_to_str() const -> std::string;

        auto tree(std::ostream&, const GraphicsParams& graphics) const -> std::ostream&;
        [[nodiscard]] auto tree_to_str(const GraphicsParams& graphics) const -> std::string;

        [[nodiscard]] auto json() const -> nlohmann::json;

        auto pretty(std::ostream&, bool show_all_builds) const -> std::ostream&;
        [[nodiscard]] auto pretty_to_str(bool show_all_builds) const -> std::string;

    private:

        using node_id = dependency_graph::node_id;
        using package_id_list = std::vector<node_id>;
        using ordered_package_list = std::map<std::string, package_id_list>;

        void reset_pkg_view_list();

        QueryType m_type;
        std::string m_query;
        dependency_graph m_dep_graph;
        package_id_list m_pkg_id_list = {};
        ordered_package_list m_ordered_pkg_id_list = {};
    };

    class Query
    {
    public:

        using Database = solver::libsolv::Database;

        [[nodiscard]] static auto find(Database& database, const std::vector<std::string>& queries)
            -> QueryResult;

        [[nodiscard]] static auto whoneeds(Database& database, std::string query, bool tree)
            -> QueryResult;

        [[nodiscard]] static auto depends(Database& database, std::string query, bool tree)
            -> QueryResult;
    };

    /********************
     *  Implementation  *
     ********************/

    constexpr auto enum_name(QueryType t) -> std::string_view
    {
        switch (t)
        {
            case QueryType::Search:
                return "Search";
            case QueryType::WhoNeeds:
                return "WhoNeeds";
            case QueryType::Depends:
                return "Depends";
        }
        throw std::invalid_argument("Invalid enum value");
    }
}
#endif
