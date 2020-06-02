#ifndef MAMBA_QUERY_HPP
#define MAMBA_QUERY_HPP

#include <functional>
#include <map>
#include <ostream>
#include <string_view>
#include <vector>

#include "output.hpp"
#include "package_info.hpp"
#include "pool.hpp"
#include "tree_util.hpp"

extern "C"
{
    #include "solv/repo.h"
    #include "solv/conda.h"
    #include "solv/solver.h"
    #include "solv/selection.h"
}

namespace nl = nlohmann;

namespace mamba
{
    std::string cut_repo_name(std::ostream& out, const std::string_view& reponame);
    void print_dep_graph(std::ostream& out, Solvable* s, const std::string& solv_str, int level, int max_level, bool last, const std::string& prefix);

    class QueryResult;

    class Query
    {
    public:

        Query(MPool& pool);

        QueryResult find(const std::string& query) const;
        QueryResult whoneeds(const std::string& query, bool tree) const;
        QueryResult depends(const std::string& query) const;

    private:

        std::reference_wrapper<MPool> m_pool;
    };

    enum class QueryType
    {
        Search,
        Depends,
        Whoneeds
    };

    class QueryResult
    {
    public:

        using package_list = std::vector<PackageInfo>;
        using package_view_list = std::vector<const PackageInfo*>;
        using package_tree = tree_node<const PackageInfo*>;
        using package_tree_ptr = std::unique_ptr<package_tree>;

        QueryResult(QueryType type,
                    const std::string& query,
                    package_list&& pkg_list);

        QueryResult(QueryType type,
                    const std::string& query,
                    package_list&& pkg_list,
                    package_tree_ptr pkg_tree);

        ~QueryResult() = default;

        QueryResult(const QueryResult&);
        QueryResult& operator=(const QueryResult&);
        QueryResult(QueryResult&&) = default;
        QueryResult& operator=(QueryResult&&) = default;

        QueryType query_type() const;
        const std::string& query() const;

        QueryResult& sort(std::string field);
        QueryResult& groupby(std::string field);
        QueryResult& reset();

        std::ostream& table(std::ostream&) const;
        std::ostream& tree(std::ostream&) const;
        nl::json json() const;

    private:

        void update_pkg_node(package_tree& node, const package_list& src);
        void reset_pkg_view_list();
        std::string get_package_repr(const PackageInfo& pkg) const;
        void print_tree_node(std::ostream& out,
                             const package_tree& node,
                             const std::string& prefix,
                             bool is_last,
                             bool root) const;

        void sort_tree_node(package_tree& node,
                            const PackageInfo::compare_fun& fun);

        QueryType m_type;
        std::string m_query;
        package_list m_pkg_list;
        package_view_list m_pkg_view_list;
        package_tree_ptr p_pkg_tree;
        using ordered_package_list = std::map<std::string, package_view_list>;
        ordered_package_list m_ordered_pkg_list;
    };
}

#endif // MAMBA_QUERY_HPP
