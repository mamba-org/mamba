// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_QUERY_HPP
#define MAMBA_CORE_QUERY_HPP

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "mamba/core/context.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/util/graph.hpp"

typedef struct s_Solvable Solvable;

namespace mamba
{
    using GraphicsParams = Context::GraphicsParams;

    void print_dep_graph(
        std::ostream& out,
        Solvable* s,
        const std::string& solv_str,
        int level,
        int max_level,
        bool last,
        const std::string& prefix
    );

    class query_result;

    class Query
    {
    public:

        Query(MPool& pool);

        query_result find(const std::vector<std::string>& queries) const;
        query_result whoneeds(const std::string& query, bool tree) const;
        query_result depends(const std::string& query, bool tree) const;

    private:

        std::reference_wrapper<MPool> m_pool;
    };

    enum class QueryType
    {
        Search,
        Depends,
        WhoNeeds
    };

    constexpr auto enum_name(QueryType t) -> std::string_view;
    auto QueryType_from_name(std::string_view name) -> QueryType;

    enum class QueryResultFormat
    {
        Json = 0,
        Tree = 1,
        Table = 2,
        Pretty = 3,
        RecursiveTable = 4,
    };

    class query_result
    {
    public:

        using dependency_graph = util::DiGraph<PackageInfo>;

        query_result(QueryType type, const std::string& query, dependency_graph&& dep_graph);

        ~query_result() = default;

        query_result(const query_result&) = default;
        query_result& operator=(const query_result&) = default;
        query_result(query_result&&) = default;
        query_result& operator=(query_result&&) = default;

        QueryType query_type() const;
        const std::string& query() const;

        query_result& sort(std::string_view field);
        query_result& groupby(std::string_view field);
        query_result& reset();

        std::ostream& table(std::ostream&) const;
        std::ostream& table(std::ostream&, const std::vector<std::string_view>& fmt) const;
        std::ostream& tree(std::ostream&, const GraphicsParams& graphics) const;
        nlohmann::json json() const;

        std::ostream& pretty(std::ostream&, const Context::OutputParams& outputParams) const;

        bool empty() const;

    private:

        using node_id = dependency_graph::node_id;
        using package_id_list = std::vector<node_id>;
        using ordered_package_list = std::map<std::string, package_id_list>;

        void reset_pkg_view_list();
        std::string get_package_repr(const PackageInfo& pkg) const;

        QueryType m_type;
        std::string m_query;
        dependency_graph m_dep_graph;
        package_id_list m_pkg_id_list = {};
        ordered_package_list m_ordered_pkg_id_list = {};
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

}  // namespace mamba

#endif  // MAMBA_QUERY_HPP
