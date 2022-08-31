// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PROBLEMS_GRAPH_HPP
#define MAMBA_PROBLEMS_GRAPH_HPP

#include <string>
#include <utility>
#include <optional>
#include <vector>
#include <regex>
#include <variant>
#include <unordered_set>

#include "mamba/core/graph_util.hpp"
#include "mamba/core/package_info.hpp"

extern "C"
{
#include "solv/solver.h"
}

#define THROWIF(condition, msg)                                                                    \
    {                                                                                              \
        \ 
        if (!condition)                                                                            \
        {                                                                                          \
            std::ostringstream os;                                                                 \
            os << msg;                                                                             \
            throw std::runtime_error(os.str());                                                    \
        }                                                                                          \
    }

#define THROW(msg)                                                                                 \
    {                                                                                              \
        std::ostringstream os;                                                                     \
        os << msg;                                                                                 \
        throw std::runtime_error(os.str());                                                        \
    }

namespace mamba
{

    enum class ProblemType
    {
        CONFLICT,
        NOT_FOUND,
        NOT_INSTALLABLE,
        BEST_NOT_INSTALLABLE,
        ONLY_DIRECT_INSTALL,
        EXCLUDED_BY_REPO_PRIORITY,
        INFERIOR_ARCH,
        PROVIDED_BY_SYSTEM
    };

    class DependencyInfo
    {
    public:
        DependencyInfo(const std::string& dep);

        const std::string& get_name() const;
        const std::string& get_range() const;
        std::string str() const;

    private:
        std::string name;
        std::string range;
    };

    namespace NodeInfo
    {
        struct ResolvedPackage
        {
            PackageInfo m_package_info;
            ResolvedPackage(PackageInfo const& package_info);
        };

        struct ProblematicPackage
        {
            std::string m_dep;
            ProblematicPackage(std::string const& dep);
        };
        struct Root
        {
        };
    }

    namespace EdgeInfo
    {
        struct Require
        {
            DependencyInfo m_dep;
            Require(DependencyInfo const& dep);
        };
        struct Constraint
        {
            DependencyInfo m_dep;
            Constraint(DependencyInfo const& dep);
        };
    }

    template <class... Ts>
    struct overload : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    overload(Ts...) -> overload<Ts...>;

    class MNode
    {
    public:
        using node_info
            = std::variant<NodeInfo::ResolvedPackage, NodeInfo::ProblematicPackage, NodeInfo::Root>;
        MNode(node_info const& node, std::optional<ProblemType> problem_type = std::nullopt);
        void maybe_update_metadata(const MNode& other);

    private:
        node_info m_info;
        std::optional<ProblemType> m_problem_type;

        bool is_root() const;
        std::string get_name() const;
        std::optional<ProblemType> get_problem_type() const;
    };

    class MEdge
    {
    public:
        using edge_info = std::variant<EdgeInfo::Require, EdgeInfo::Constraint>;

        MEdge(edge_info const& info);
        std::string get_info() const;

    private:
        edge_info m_info;
    };

    template <typename T, typename U>
    class MProblemsGraph
    {
    public:
        using graph_t = DiGraph<T, U>;
        using node_id = typename DiGraph<T, U>::node_id;
        using node_id_conflicts = std::unordered_map<node_id, std::vector<node_id>>;

        graph_t& graph();
        void add_conflicts(node_id node1, node_id node2);
        node_id_conflicts const& get_conflicts() const;

    private:
        graph_t m_graph;
        node_id_conflicts m_node_id_conflicts;
    };
}

#endif  // MAMBA_PROBLEMS_GRAPH_HPP
