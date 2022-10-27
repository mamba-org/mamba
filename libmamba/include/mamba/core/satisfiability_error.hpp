// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PROBLEMS_GRAPH_HPP
#define MAMBA_PROBLEMS_GRAPH_HPP

#include <string>
#include <utility>
#include <variant>
#include <unordered_map>
#include <optional>
#include <vector>

#include <solv/solver.h>

#include "mamba/core/util_graph.hpp"
#include "mamba/core/package_info.hpp"

namespace mamba
{

    class MSolver;
    class MPool;

    /**
     * Separate a dependency spec into a package name and the version range.
     */
    class DependencyInfo
    {
    public:
        DependencyInfo(const std::string& dependency);

        DependencyInfo(DependencyInfo const&) = default;
        DependencyInfo(DependencyInfo&&) noexcept = default;
        DependencyInfo& operator=(DependencyInfo const&) = default;
        DependencyInfo& operator=(DependencyInfo&&) noexcept = default;

        const std::string& name() const;
        const std::string& version() const;
        const std::string& build_string() const;
        std::string str() const;

        bool operator==(DependencyInfo const& other) const;

    private:
        std::string m_name;
        std::string m_version_range;
        std::string m_build_range;
    };

    template <typename T>
    class conflict_map : private std::unordered_map<T, vector_set<T>>
    {
    public:
        using Base = std::unordered_map<T, vector_set<T>>;
        using typename Base::const_iterator;
        using typename Base::key_type;
        using typename Base::value_type;

        using Base::empty;
        using Base::size;
        bool has_conflict(key_type const& a) const;
        auto conflicts(key_type const& a) const -> vector_set<T> const&;
        bool in_conflict(key_type const& a, key_type const& b) const;

        using Base::cbegin;
        using Base::cend;
        const_iterator begin() const noexcept;
        const_iterator end() const noexcept;

        using Base::clear;
        void add(key_type const& a, key_type const& b);
    };

    /**
     * A directed graph of the packages involved in a libsolv conflict.
     */
    class ProblemsGraph
    {
    public:
        struct RootNode
        {
        };
        struct PackageNode : PackageInfo
        {
            std::optional<SolverRuleinfo> problem_type;

            PackageNode(PackageNode const&) = default;
            PackageNode(PackageNode&&) noexcept = default;
            PackageNode& operator=(PackageNode const&) = default;
            PackageNode& operator=(PackageNode&&) noexcept = default;
        };
        struct UnresolvedDependencyNode : DependencyInfo
        {
            static SolverRuleinfo constexpr problem_type = SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP;

            UnresolvedDependencyNode(UnresolvedDependencyNode const&) = default;
            UnresolvedDependencyNode(UnresolvedDependencyNode&&) noexcept = default;
            UnresolvedDependencyNode& operator=(UnresolvedDependencyNode const&) = default;
            UnresolvedDependencyNode& operator=(UnresolvedDependencyNode&&) noexcept = default;
        };
        struct ConstraintNode : DependencyInfo
        {
            static SolverRuleinfo constexpr problem_type = SOLVER_RULE_PKG_CONSTRAINS;

            ConstraintNode(ConstraintNode const&) = default;
            ConstraintNode(ConstraintNode&&) noexcept = default;
            ConstraintNode& operator=(ConstraintNode const&) = default;
            ConstraintNode& operator=(ConstraintNode&&) noexcept = default;
        };
        using node_t
            = std::variant<RootNode, PackageNode, UnresolvedDependencyNode, ConstraintNode>;

        using edge_t = DependencyInfo;

        using graph_t = DiGraph<node_t, edge_t>;
        using node_id = graph_t::node_id;
        using conflicts_t = conflict_map<node_id>;

        static ProblemsGraph from_solver(MSolver const& solver, MPool const& pool);

        ProblemsGraph(graph_t graph, conflicts_t conflicts, node_id root_node);

        graph_t const& graph() const noexcept;
        conflicts_t const& conflicts() const noexcept;
        node_id root_node() const noexcept;

    private:
        graph_t m_graph;
        conflicts_t m_conflicts;
        node_id m_root_node;
    };

    class CompressedProblemsGraph
    {
    public:
        using RootNode = ProblemsGraph::RootNode;

        /**
         * A list of objects with a name, version, and build member.
         *
         * For simplicity, the implementation is kept in the translation unit with
         * specialization for needed types.
         */
        template <typename T, typename Allocator = std::allocator<T>>
        class NamedList : private std::vector<T, Allocator>
        {
        public:
            using Base = std::vector<T, Allocator>;
            using typename Base::allocator_type;
            using typename Base::const_iterator;
            using typename Base::const_reverse_iterator;
            using typename Base::value_type;

            using Base::empty;
            using Base::size;
            value_type const& front() const noexcept;
            value_type const& back() const noexcept;
            using Base::cbegin;
            using Base::cend;
            using Base::crbegin;
            using Base::crend;
            const_iterator begin() const noexcept;
            const_iterator end() const noexcept;
            const_reverse_iterator rbegin() const noexcept;
            const_reverse_iterator rend() const noexcept;

            std::string const& name() const;
            std::string versions_trunc() const;
            std::string build_strings_trunc() const;

            using Base::clear;
            using Base::reserve;
            void push_back(value_type const& e);
            void push_back(value_type&& e);

        private:
            template <typename T_>
            void push_back_impl(T_&& e);
        };

        using PackageListNode = NamedList<ProblemsGraph::PackageNode>;
        using UnresolvedDependencyListNode = NamedList<ProblemsGraph::UnresolvedDependencyNode>;
        using ConstraintListNode = NamedList<ProblemsGraph::ConstraintNode>;
        using node_t = std::variant<RootNode,  //
                                    PackageListNode,
                                    UnresolvedDependencyListNode,
                                    ConstraintListNode>;

        using edge_t = NamedList<DependencyInfo>;

        using graph_t = DiGraph<node_t, edge_t>;
        using node_id = graph_t::node_id;
        using conflicts_t = conflict_map<node_id>;

        static auto from_problems_graph(ProblemsGraph const& pbs) -> CompressedProblemsGraph;

        CompressedProblemsGraph(graph_t graph, conflicts_t conflicts, node_id root_node);

        graph_t const& graph() const noexcept;
        conflicts_t const& conflicts() const noexcept;
        node_id root_node() const noexcept;

    private:
        graph_t m_graph;
        conflicts_t m_conflicts;
        node_id m_root_node;
    };

    /************************************
     *  Implementation of conflict_map  *
     ************************************/

    template <typename T>
    bool conflict_map<T>::has_conflict(key_type const& a) const
    {
        return Base::find(a) != end();
    }

    template <typename T>
    auto conflict_map<T>::conflicts(key_type const& a) const -> vector_set<T> const&
    {
        return Base::at(a);
    }

    template <typename T>
    bool conflict_map<T>::in_conflict(key_type const& a, key_type const& b) const
    {
        return has_conflict(a) && Base::at(a).contains(b);
    }

    template <typename T>
    auto conflict_map<T>::begin() const noexcept -> const_iterator
    {
        return Base::begin();
    }

    template <typename T>
    auto conflict_map<T>::end() const noexcept -> const_iterator
    {
        return Base::end();
    }

    template <typename T>
    void conflict_map<T>::add(key_type const& a, key_type const& b)
    {
        Base::operator[](a).insert(b);
        Base::operator[](b).insert(a);
    }
}

#endif  // MAMBA_PROBLEMS_GRAPH_HPP
