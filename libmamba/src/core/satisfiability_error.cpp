// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include "mamba/core/satisfiability_error.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    ProblemsGraph::ProblemsGraph(graph_t graph, conflicts_t conflicts, node_id root_node)
        : m_graph(std::move(graph))
        , m_conflicts(std::move(conflicts))
        , m_root_node(root_node)
    {
    }

    auto ProblemsGraph::graph() const noexcept -> const graph_t&
    {
        return m_graph;
    }

    auto ProblemsGraph::conflicts() const noexcept -> const conflicts_t&
    {
        return m_conflicts;
    }

    auto ProblemsGraph::root_node() const noexcept -> node_id
    {
        return m_root_node;
    }

    /******************************************
     *  Implementation of simplify_conflicts  *
     ******************************************/

    ProblemsGraph simplify_conflicts(const ProblemsGraph& pbs)
    {
        using node_id = ProblemsGraph::node_id;
        using node_t = ProblemsGraph::node_t;

        const auto& old_graph = pbs.graph();
        const auto& old_conflicts = pbs.conflicts();

        const auto is_constraint = [](const node_t& node) -> bool
        { return std::holds_alternative<ProblemsGraph::ConstraintNode>(node); };

        const auto has_constraint_child = [&](node_id n)
        {
            if (old_graph.out_degree(n) == 1)
            {
                node_id const s = old_graph.successors(n).front();
                // If s is of constraint type, it has conflicts
                assert(!is_constraint(old_graph.node(s)) || old_conflicts.has_conflict(s));
                return is_constraint(old_graph.node(s));
            }
            return false;
        };

        // Working on a copy since we cannot safely iterate through object
        // and modify them at the same time
        auto graph = pbs.graph();
        auto conflicts = pbs.conflicts();

        for (const auto& [id, id_conflicts] : old_conflicts)
        {
            // We are trying to detect node that are in conflicts but are not leaves.
            // This shows up in Pyhon dependencies because the constraint on ``python`` and
            // ``python_abi`` was reversed.
            if (has_constraint_child(id))
            {
                assert(old_graph.out_degree(id) == 1);
                const node_id id_child = old_graph.successors(id).front();
                for (const auto& c : id_conflicts)
                {
                    // Since ``id`` has a constraint node child (``id-child``) and is itself in
                    // conflict with another constraint node ``c``, we are moving the conflict
                    // between the ``id_child`` and the parents ``c_parent`` of the ``c``.
                    // This is a bit hacky but the intuition is to replicate the structure of
                    // nodes conflicting with ``id_child`` with ``c_parent``.
                    // They likely reprensent the same thing and so we want them to be able to
                    // merge later on.

                    // id_child may alrady have been removed through a preivous iteration
                    if (graph.has_node(id_child) && is_constraint(old_graph.node(c)))
                    {
                        for (const node_id c_parent : old_graph.predecessors(c))
                        {
                            assert(graph.has_node(id_child));
                            assert(graph.has_node(c_parent));
                            conflicts.add(id_child, c_parent);
                            graph.remove_edge(c_parent, c);
                        }
                        conflicts.remove(id, c);
                        if (!conflicts.has_conflict(c))
                        {
                            graph.remove_node(c);
                        }
                    }
                }
            }
        }
        return { std::move(graph), std::move(conflicts), pbs.root_node() };
    }

    /***********************************************
     *  Implementation of CompressedProblemsGraph  *
     ***********************************************/

    namespace
    {
        using old_node_id_list = std::vector<ProblemsGraph::node_id>;

        /**
         * Merge node indices for the a given type of node.
         *
         * This function is applied among node of the same type to compute the indices of nodes
         * that will be merged together.
         *
         * @param node_indices The indices of nodes of a given type.
         * @param merge_criteria A binary function that decices whether two nodes should be merged
         *        together. The function is assumed to be symetric and transitive.
         * @return A partition of the the indices in @p node_indices.
         */
        template <typename CompFunc>
        auto merge_node_indices_for_one_node_type(
            const std::vector<ProblemsGraph::node_id>& node_indices,
            CompFunc&& merge_criteria
        ) -> std::vector<old_node_id_list>
        {
            using node_id = ProblemsGraph::node_id;

            std::vector<old_node_id_list> groups{};

            const std::size_t n_nodes = node_indices.size();
            std::vector<bool> node_added_to_a_group(n_nodes, false);
            for (std::size_t i = 0; i < n_nodes; ++i)
            {
                if (!node_added_to_a_group[i])
                {
                    const auto id_i = node_indices[i];
                    std::vector<node_id> current_group{};
                    current_group.push_back(id_i);
                    node_added_to_a_group[i] = true;
                    // This is where we use symetry and transitivity, going through all remaining
                    // nodes and adding them to the current group if they match the criteria.
                    for (std::size_t j = i + 1; j < n_nodes; ++j)
                    {
                        const auto id_j = node_indices[j];
                        if ((!node_added_to_a_group[j]) && merge_criteria(id_i, id_j))
                        {
                            current_group.push_back(id_j);
                            node_added_to_a_group[j] = true;
                        }
                    }
                    groups.push_back(std::move(current_group));
                }
            }
            assert(std::all_of(
                node_added_to_a_group.begin(),
                node_added_to_a_group.end(),
                [](auto x) { return x; }
            ));
            return groups;
        }

        /**
         * Used to organize objects by node type (the index of the type in the variant).
         */
        template <typename T>
        using node_type_list = std::vector<T>;

        /**
         * Group ids of nodes with same alternative together.
         *
         * If a node variant at with id ``id`` in the input graph @p g holds alternative
         * with variant index ``k`` then the output will contain ``id`` in position ``k``.
         */
        auto node_id_by_type(const ProblemsGraph::graph_t& g) -> node_type_list<old_node_id_list>
        {
            using node_id = ProblemsGraph::node_id;
            using node_t = ProblemsGraph::node_t;
            static constexpr auto n_types = std::variant_size_v<node_t>;

            auto out = node_type_list<old_node_id_list>(n_types);
            g.for_each_node_id([&](node_id id) { out[g.node(id).index()].push_back(id); });

            return out;
        }

        /**
         * Merge node indices together.
         *
         * Merge by applying the @p merge_criteria to nodes that hold the same type in the variant.
         *
         * @param merge_criteria A binary function that decices whether two nodes should be merged
         * together. The function is assumed to be symetric and transitive.
         * @return For each node type, a partition of the the indices in @p of that type..
         */
        template <typename CompFunc>
        auto
        merge_node_indices(const node_type_list<old_node_id_list>& nodes_by_type, CompFunc&& merge_criteria)
            -> node_type_list<std::vector<old_node_id_list>>
        {
            auto merge_func = [&merge_criteria](const auto& node_indices_of_one_node_type)
            {
                return merge_node_indices_for_one_node_type(
                    node_indices_of_one_node_type,
                    std::forward<CompFunc>(merge_criteria)
                );
            };
            node_type_list<std::vector<old_node_id_list>> groups(nodes_by_type.size());
            std::transform(nodes_by_type.begin(), nodes_by_type.end(), groups.begin(), merge_func);
            return groups;
        }

        /**
         * Given a variant type and a type, return the index of that type in the variant.
         *
         * Conceptually the opposite of ``std::variant_alternative``.
         * Credits to Nykakin on StackOverflow (https://stackoverflow.com/a/52303671).
         */
        template <typename VariantType, typename T, std::size_t index = 0>
        constexpr std::size_t variant_type_index()
        {
            static_assert(std::variant_size_v<VariantType> > index, "Type not found in variant");
            if constexpr (std::is_same_v<std::variant_alternative_t<index, VariantType>, T>)
            {
                return index;
            }
            else
            {
                return variant_type_index<VariantType, T, index + 1>();
            }
        }

        /**
         * ``ranges::transform(f) | ranges::to<CompressedProblemsGraph::NamedList>()``
         */
        template <typename Range, typename Func>
        auto transform_to_list(const Range& rng, Func&& f)
        {
            using T = typename Range::value_type;
            using O = std::invoke_result_t<Func, T>;
            // TODO(C++20) ranges::view::transform
            auto tmp = std::vector<O>();
            tmp.reserve(rng.size());
            std::transform(rng.begin(), rng.end(), std::back_inserter(tmp), std::forward<Func>(f));
            return CompressedProblemsGraph::NamedList<O>(tmp.begin(), tmp.end());
        }

        template <typename T>
        auto invoke_version(T&& e) -> decltype(auto)
        {
            using TT = std::remove_cv_t<std::remove_reference_t<T>>;
            using Ver = decltype(std::invoke(&TT::version, std::forward<T>(e)));
            Ver v = std::invoke(&TT::version, std::forward<T>(e));
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, specs::VersionSpec>)
            {
                return std::forward<Ver>(v).str();
            }
            else
            {
                return v;
            }
        }

        template <typename T>
        auto invoke_build_string(T&& e) -> decltype(auto)
        {
            using TT = std::remove_cv_t<std::remove_reference_t<T>>;
            using Build = decltype(std::invoke(&TT::build_string, std::forward<T>(e)));
            Build bld = std::invoke(&TT::build_string, std::forward<T>(e));
            if constexpr (std::is_same_v<std::decay_t<decltype(bld)>, specs::GlobSpec>)
            {
                return std::forward<Build>(bld).str();
            }
            else
            {
                return bld;
            }
        }

        template <typename T>
        auto invoke_name(T&& e) -> decltype(auto)
        {
            using TT = std::remove_cv_t<std::remove_reference_t<T>>;
            using Name = decltype(std::invoke(&TT::name, std::forward<T>(e)));
            Name name = std::invoke(&TT::name, std::forward<T>(e));
            if constexpr (std::is_same_v<std::decay_t<decltype(name)>, specs::GlobSpec>)
            {
                return std::forward<Name>(name).str();
            }
            else
            {
                return name;
            }
        }

        /**
         * Detect if a type has a ``name`` member (function).
         */
        template <class T, class = void>
        struct has_name : std::false_type
        {
        };

        template <class T>
        struct has_name<T, std::void_t<decltype(std::invoke(&T::name, std::declval<T>()))>>
            : std::true_type
        {
        };

        template <typename T>
        inline constexpr bool has_name_v = has_name<T>::value;

        template <typename T, typename Str>
        decltype(auto) name_or(const T& obj, Str val)
        {
            if constexpr (has_name_v<T>)
            {
                return invoke_name(obj);
            }
            else
            {
                return val;
            }
        }

        /**
         * The name of a ProblemsGraph::node_t, used to avoid merging.
         */
        std::string_view node_name(const ProblemsGraph::node_t& node)
        {
            return std::visit([](const auto& n) -> std::string_view { return name_or(n, ""); }, node);
        }

        /**
         * The criteria for deciding whether to merge two nodes together.
         */
        auto default_merge_criteria(
            const ProblemsGraph& pbs,
            ProblemsGraph::node_id n1,
            ProblemsGraph::node_id n2
        ) -> bool
        {
            using node_id = ProblemsGraph::node_id;
            const auto& g = pbs.graph();
            auto is_leaf = [&g](node_id n) -> bool { return g.successors(n).size() == 0; };
            auto leaves_from = [&g](node_id n) -> util::flat_set<node_id>
            {
                auto leaves = std::vector<node_id>();
                g.for_each_leaf_id_from(n, [&leaves](node_id m) { leaves.push_back(m); });
                return util::flat_set(std::move(leaves));
            };
            return (node_name(g.node(n1)) == node_name(g.node(n2)))
                   // Merging conflicts would be counter-productive in explaining problems
                   && !(pbs.conflicts().in_conflict(n1, n2))
                   // We don't want to use leaves_from for leaves because it resolve to themselves,
                   // preventing any merging.
                   && ((is_leaf(n1) && is_leaf(n2)) || (leaves_from(n1) == leaves_from(n2)))
                   // We only check the parents for leaves meaning parents can "inject"
                   // themselves into a bigger problem
                   && ((!is_leaf(n1) && !is_leaf(n2)) || (g.predecessors(n1) == g.predecessors(n2)));
        }

        using node_id_mapping = std::map<ProblemsGraph::node_id, CompressedProblemsGraph::node_id>;

        /**
         * For a given type of node, merge nodes together and add them to the new graph.
         *
         * @param old_graph The graph containing the node data referenced by @p old_groups.
         * @param old_groups A partition of the node indices to merge together.
         * @param new_graph The graph where the merged nodes are added.
         * @param old_to_new A mapping of old node indices to new node indices updated with the new
         * nodes merged.
         */
        template <typename Node>
        void merge_nodes_for_one_node_type(
            const ProblemsGraph::graph_t& old_graph,
            const std::vector<old_node_id_list>& old_groups,
            CompressedProblemsGraph::graph_t& new_graph,
            node_id_mapping& old_to_new
        )
        {
            auto get_old_node = [&old_graph](ProblemsGraph::node_id id)
            {
                auto node = old_graph.node(id);
                // Must always be the case that old_groups only references node ids of type Node
                assert(std::holds_alternative<Node>(node));
                return std::get<Node>(std::move(node));
            };

            for (const auto& old_grp : old_groups)
            {
                const auto new_id = new_graph.add_node(transform_to_list(old_grp, get_old_node));
                for (const auto old_id : old_grp)
                {
                    old_to_new[old_id] = new_id;
                }
            }
        }

        /**
         * Merge nodes together.
         *
         * Merge by applying the @p merge_criteria to nodes that hold the same type in the variant.
         *
         * @param merge_criteria A binary function that decices whether two nodes should be merged
         * together. The function is assumed to be symetric and transitive.
         * @return A tuple of the graph with newly created nodes (without edges), the new root node,
         * and a mapping between old node ids and new node ids.
         */
        template <typename CompFunc>
        auto merge_nodes(const ProblemsGraph& pbs, CompFunc&& merge_criteria)
            -> std::tuple<CompressedProblemsGraph::graph_t, CompressedProblemsGraph::node_id, node_id_mapping>
        {
            const auto& old_graph = pbs.graph();
            auto new_graph = CompressedProblemsGraph::graph_t();
            const auto new_root_node = new_graph.add_node(CompressedProblemsGraph::RootNode());

            auto old_to_new = node_id_mapping{};

            auto old_ids_groups = merge_node_indices(
                node_id_by_type(pbs.graph()),
                std::forward<CompFunc>(merge_criteria)
            );

            {
                using Node = ProblemsGraph::RootNode;
                [[maybe_unused]] static constexpr auto type_idx = variant_type_index<ProblemsGraph::node_t, Node>(
                );
                assert(old_ids_groups[type_idx].size() == 1);
                assert(old_ids_groups[type_idx][0].size() == 1);
                assert(old_ids_groups[type_idx][0][0] == pbs.root_node());
                old_to_new[pbs.root_node()] = new_root_node;
            }
            {
                using Node = ProblemsGraph::PackageNode;
                static constexpr auto type_idx = variant_type_index<ProblemsGraph::node_t, Node>();
                merge_nodes_for_one_node_type<Node>(
                    old_graph,
                    old_ids_groups[type_idx],
                    new_graph,
                    old_to_new
                );
            }
            {
                using Node = ProblemsGraph::UnresolvedDependencyNode;
                static constexpr auto type_idx = variant_type_index<ProblemsGraph::node_t, Node>();
                merge_nodes_for_one_node_type<Node>(
                    old_graph,
                    old_ids_groups[type_idx],
                    new_graph,
                    old_to_new
                );
            }
            {
                using Node = ProblemsGraph::ConstraintNode;
                static constexpr auto type_idx = variant_type_index<ProblemsGraph::node_t, Node>();
                merge_nodes_for_one_node_type<Node>(
                    old_graph,
                    old_ids_groups[type_idx],
                    new_graph,
                    old_to_new
                );
            }
            return std::tuple{ std::move(new_graph), new_root_node, std::move(old_to_new) };
        }

        /**
         * For all groups in the new graph, merge the edges in-between nodes of those groups.
         *
         * @param old_graph The graph with the nodes use for merging.
         * @param new_graph The graph with nodes already merged, modified to add new edges.
         * @param old_to_new A mapping between old node ids and new node ids.
         */
        void merge_edges(
            const ProblemsGraph::graph_t& old_graph,
            CompressedProblemsGraph::graph_t& new_graph,
            const node_id_mapping& old_to_new
        )
        {
            auto add_new_edge = [&](ProblemsGraph::node_id old_from, ProblemsGraph::node_id old_to)
            {
                auto const new_from = old_to_new.at(old_from);
                auto const new_to = old_to_new.at(old_to);
                if (!new_graph.has_edge(new_from, new_to))
                {
                    new_graph.add_edge(new_from, new_to, CompressedProblemsGraph::edge_t());
                }
                new_graph.edge(new_from, new_to).insert(old_graph.edge(old_from, old_to));
            };
            old_graph.for_each_edge_id(add_new_edge);
        }

        /**
         * Merge the conflicts according to the new groups of nodes.
         *
         * If two groups contain a node that are respectively in conflicts, then they are in
         * conflicts.
         */
        auto
        merge_conflicts(const ProblemsGraph::conflicts_t& old_conflicts, const node_id_mapping& old_to_new)
            -> CompressedProblemsGraph::conflicts_t
        {
            auto new_conflicts = CompressedProblemsGraph::conflicts_t();
            for (const auto& [old_from, old_with] : old_conflicts)
            {
                const auto new_from = old_to_new.at(old_from);
                for (const auto old_to : old_with)
                {
                    new_conflicts.add(new_from, old_to_new.at(old_to));
                }
            }
            return new_conflicts;
        }
    }

    // TODO move graph nodes and edges.
    auto CompressedProblemsGraph::from_problems_graph(
        const ProblemsGraph& pbs,
        const merge_criteria_t& merge_criteria
    ) -> CompressedProblemsGraph
    {
        graph_t graph = {};
        node_id root_node = {};
        node_id_mapping old_to_new = {};
        if (merge_criteria)
        {
            auto merge_func =
                [&pbs, &merge_criteria](ProblemsGraph::node_id n1, ProblemsGraph::node_id n2)
            { return merge_criteria(pbs, n1, n2); };
            std::tie(graph, root_node, old_to_new) = merge_nodes(pbs, merge_func);
        }
        else
        {
            auto merge_func = [&pbs](ProblemsGraph::node_id n1, ProblemsGraph::node_id n2)
            { return default_merge_criteria(pbs, n1, n2); };
            std::tie(graph, root_node, old_to_new) = merge_nodes(pbs, merge_func);
        }
        merge_edges(pbs.graph(), graph, old_to_new);
        auto conflicts = merge_conflicts(pbs.conflicts(), old_to_new);
        return { std::move(graph), std::move(conflicts), root_node };
    }

    CompressedProblemsGraph::CompressedProblemsGraph(graph_t graph, conflicts_t conflicts, node_id root_node)
        : m_graph(std::move(graph))
        , m_conflicts(std::move(conflicts))
        , m_root_node(root_node)
    {
    }

    auto CompressedProblemsGraph::graph() const noexcept -> const graph_t&
    {
        return m_graph;
    }

    auto CompressedProblemsGraph::conflicts() const noexcept -> const conflicts_t&
    {
        return m_conflicts;
    }

    auto CompressedProblemsGraph::root_node() const noexcept -> node_id
    {
        return m_root_node;
    }

    /*************************************************************
     *  Implementation of CompressedProblemsGraph::RoughCompare  *
     *************************************************************/

    template <typename T>
    auto CompressedProblemsGraph::RoughCompare<T>::operator()(const T& a, const T& b) const -> bool
    {
        auto attrs = [](const auto& x)
        {
            using Attrs = std::tuple<
                decltype(invoke_name(x)),
                decltype(invoke_version(x)),
                decltype(std::invoke(&T::build_number, x)),
                decltype(invoke_build_string(x))>;
            return Attrs(
                invoke_name(x),
                invoke_version(x),
                std::invoke(&T::build_number, x),
                invoke_build_string(x)
            );
        };
        return attrs(a) < attrs(b);
    }

    template struct CompressedProblemsGraph::RoughCompare<ProblemsGraph::PackageNode>;
    template struct CompressedProblemsGraph::RoughCompare<ProblemsGraph::UnresolvedDependencyNode>;
    template struct CompressedProblemsGraph::RoughCompare<ProblemsGraph::ConstraintNode>;
    template struct CompressedProblemsGraph::RoughCompare<specs::MatchSpec>;

    /**********************************************************
     *  Implementation of CompressedProblemsGraph::NamedList  *
     **********************************************************/

    template <typename T, typename A>
    template <typename InputIterator>
    CompressedProblemsGraph::NamedList<T, A>::NamedList(InputIterator first, InputIterator last)
    {
        if (first < last)
        {
            for (auto it = first; it < last; ++it)
            {
                if (invoke_name(*it) != invoke_name(*first))
                {
                    throw std::invalid_argument(util::concat(
                        "iterator contains different names (",
                        invoke_name(*first),
                        ", ",
                        invoke_name(*it)
                    ));
                }
            }
        }
        Base::insert(first, last);
    }

    template <typename T, typename A>
    auto CompressedProblemsGraph::NamedList<T, A>::front() const noexcept -> const value_type&
    {
        return Base::front();
    }

    template <typename T, typename A>
    auto CompressedProblemsGraph::NamedList<T, A>::back() const noexcept -> const value_type&
    {
        return Base::back();
    }

    template <typename T, typename A>
    auto CompressedProblemsGraph::NamedList<T, A>::begin() const noexcept -> const_iterator
    {
        return Base::begin();
    }

    template <typename T, typename A>
    auto CompressedProblemsGraph::NamedList<T, A>::end() const noexcept -> const_iterator
    {
        return Base::end();
    }

    template <typename T, typename A>
    auto CompressedProblemsGraph::NamedList<T, A>::rbegin() const noexcept -> const_reverse_iterator
    {
        return Base::rbegin();
    }

    template <typename T, typename A>
    auto CompressedProblemsGraph::NamedList<T, A>::rend() const noexcept -> const_reverse_iterator
    {
        return Base::rend();
    }

    template <typename T, typename A>
    auto CompressedProblemsGraph::NamedList<T, A>::name() const -> const std::string&
    {
        if (size() == 0)
        {
            static const std::string lempty = "";
            return lempty;
        }
        return invoke_name(front());
    }

    template <typename T, typename A>
    auto CompressedProblemsGraph::NamedList<T, A>::versions_trunc(
        std::string_view sep,
        std::string_view etc,
        std::size_t threshold,
        bool remove_duplicates
    ) const -> std::pair<std::string, std::size_t>
    {
        auto versions = std::vector<std::string>(size());
        // TODO(C++20) *this | std::ranges::transform(invoke_version) | ranges::unique
        std::transform(
            begin(),
            end(),
            versions.begin(),
            [](const auto& x) { return invoke_version(x); }
        );
        if (remove_duplicates)
        {
            versions.erase(std::unique(versions.begin(), versions.end()), versions.end());
        }
        return { util::join_trunc(versions, sep, etc, threshold), versions.size() };
    }

    template <typename T, typename A>
    auto CompressedProblemsGraph::NamedList<T, A>::build_strings_trunc(
        std::string_view sep,
        std::string_view etc,
        std::size_t threshold,
        bool remove_duplicates
    ) const -> std::pair<std::string, std::size_t>
    {
        auto builds = std::vector<std::string>(size());
        // TODO(C++20) *this | std::ranges::transform(invoke_buid_string) | ranges::unique
        std::transform(
            begin(),
            end(),
            builds.begin(),
            [](const auto& p) { return invoke_build_string(p); }
        );
        if (remove_duplicates)
        {
            builds.erase(std::unique(builds.begin(), builds.end()), builds.end());
        }
        return { util::join_trunc(builds, sep, etc, threshold), builds.size() };
    }

    template <typename T, typename A>
    auto CompressedProblemsGraph::NamedList<T, A>::versions_and_build_strings_trunc(
        std::string_view sep,
        std::string_view etc,
        std::size_t threshold,
        bool remove_duplicates
    ) const -> std::pair<std::string, std::size_t>
    {
        auto versions_builds = std::vector<std::string>(size());
        auto invoke_version_builds = [](auto&& v) -> decltype(auto)
        { return fmt::format("{} {}", invoke_version(v), invoke_build_string(v)); };
        // TODO(C++20) *this | std::ranges::transform(invoke_version) | ranges::unique
        std::transform(begin(), end(), versions_builds.begin(), invoke_version_builds);
        if (remove_duplicates)
        {
            versions_builds.erase(
                std::unique(versions_builds.begin(), versions_builds.end()),
                versions_builds.end()
            );
        }
        return { util::join_trunc(versions_builds, sep, etc, threshold), versions_builds.size() };
    }

    template <typename T, typename A>
    void CompressedProblemsGraph::NamedList<T, A>::insert(const value_type& e)
    {
        return insert_impl(e);
    }

    template <typename T, typename A>
    void CompressedProblemsGraph::NamedList<T, A>::insert(value_type&& e)
    {
        return insert_impl(std::move(e));
    }

    template <typename T, typename A>
    template <typename T_>
    void CompressedProblemsGraph::NamedList<T, A>::insert_impl(T_&& e)
    {
        if ((size() > 0) && (invoke_name(e) != name()))
        {
            throw std::invalid_argument(
                "Name of new element (" + invoke_name(e) + ") does not match name of list ("
                + name() + ')'
            );
        }
        Base::insert(std::forward<T_>(e));
    }

    template class CompressedProblemsGraph::NamedList<ProblemsGraph::PackageNode>;
    template class CompressedProblemsGraph::NamedList<ProblemsGraph::UnresolvedDependencyNode>;
    template class CompressedProblemsGraph::NamedList<ProblemsGraph::ConstraintNode>;
    template class CompressedProblemsGraph::NamedList<specs::MatchSpec>;

    /***********************************
     *  Implementation of summary_msg  *
     ***********************************/

    /****************************************
     *  Implementation of problem_tree_msg  *
     ****************************************/

    namespace
    {

        /**
         * Describes how a given graph node appears in the DFS.
         */
        struct TreeNode
        {
            enum struct Type
            {
                /** A root node with no ancestors and at least one successor. */
                root,
                /** A leaf node with at least one ancestors and no successor. */
                leaf,
                /** A node with at least one ancestor that has already been visited */
                visited,
                /** Start dependency split (multiple edges with same dep_id). */
                split,
                /** A regular node with at least one ancestor and at least one successor. */
                diving
            };

            /**
             * Keep track of whether a node is the last visited among its sibling.
             */
            enum struct SiblingNumber : bool
            {
                not_last = 0,
                last = 1,
            };

            /** Progagate a status up the tree, such as whether the package is installable. */
            using Status = bool;

            using node_id = CompressedProblemsGraph::node_id;

            std::vector<SiblingNumber> ancestry;
            /** Multiple node_id means that the node is "virtual".
             *
             * It represents a group of node, as in a split (but other types are used as well).
             */
            std::vector<node_id> ids;
            std::vector<node_id> ids_from;
            Type type;
            Type type_from;
            Status status;

            auto depth() const -> std::size_t
            {
                return ancestry.size();
            }
        };

        /**
         * The depth first search (DFS) algorithm to explain problems.
         *
         * We need to reimplement a DFS algorithm instead of using the one provided by the one
         * from DiGraph because we need a more complex exploration, including:
         *   - Controling the order in which neighbors are explored;
         *   - Dynamically adding and removing nodes;
         *   - Executing some operations before and after a node is visited;
         *   - Propagating information from the exploration of the subtree back to the current
         *     node (e.g. the status).
         *
         * The goal of this class is to return a vector of ``TreeNode``, *i.e.* a ``node_id``
         * enhanced with extra DFS information.
         * This is not where string representation is done.
         *
         * A first step in mitigating these constraints would be to put more structure in the
         * graph to start with.
         * Currently a node ``pkg_a-0.1.0-build`` depends directly on other packages such as
         *   - ``pkg_b-0.3.0-build``
         *   - ``pkg_b-0.3.1-build``
         *   - ``pkg_c-0.2.0-build``
         * One and only one of ``pkg_b`` and ``pkg_c`` must be selected, which is why we have
         * to first do a grouping by package name.
         * A more structured approach would be to put an intermerdiary "dependency" node
         * (currently represented by the edge data) that would serve as grouping packages with the
         * same name together.
         */
        class TreeDFS
        {
        public:

            /**
             * Initialize search data and capture reference to the problems.
             */
            TreeDFS(const CompressedProblemsGraph& pbs);

            /**
             * Execute DFS and return the vector of ``TreeNode``.
             */
            auto explore() -> std::vector<TreeNode>;

        private:

            using graph_t = CompressedProblemsGraph::graph_t;
            using node_id = CompressedProblemsGraph::node_id;
            using Status = TreeNode::Status;
            using SiblingNumber = TreeNode::SiblingNumber;
            using TreeNodeList = std::vector<TreeNode>;
            using TreeNodeIter = typename TreeNodeList::iterator;

            util::flat_set<node_id> leaf_installables = {};
            std::map<node_id, std::optional<Status>> m_node_visited = {};
            const CompressedProblemsGraph& m_pbs;

            /**
             * Function to decide if a node is not installable.
             *
             * For a leaf this sets the final status.
             * For other nodes, a pacakge could still be not installable because of its children.
             */
            auto node_not_installable(node_id id) -> Status;

            /**
             * Get the type of a node depending on the exploration.
             */
            auto node_type(node_id id) const -> TreeNode::Type;

            /**
             * The successors of a node, grouped by same dependency name (edge data).
             */
            auto successors_per_dep(node_id from, bool name_only);

            /**
             * Visit a "split" node.
             *
             * A node that aims at grouping versions and builds of a given dependency.
             * Exactly the missing information that should be added to the graph as a proper node.
             */
            auto visit_split(
                const std::vector<node_id>& children_ids,
                SiblingNumber position,
                const TreeNode& from,
                TreeNodeIter out
            ) -> std::pair<TreeNodeIter, Status>;
            /**
             * Visit a node from another node.
             */
            auto
            visit_node(node_id id, SiblingNumber position, const TreeNode& from, TreeNodeIter out)
                -> std::pair<TreeNodeIter, Status>;
            /**
             * Visit the first node in the graph.
             */
            auto visit_node(node_id id, TreeNodeIter out) -> std::pair<TreeNodeIter, Status>;
            /**
             * Code reuse.
             */
            auto visit_node_impl(node_id id, const TreeNode& ongoing, TreeNodeIter out)
                -> std::pair<TreeNodeIter, Status>;
        };

        /*******************************
         *  Implementation of TreeDFS  *
         *******************************/

        TreeDFS::TreeDFS(const CompressedProblemsGraph& pbs)
            : m_pbs(pbs)
        {
            pbs.graph().for_each_node_id([&](auto id) { m_node_visited.emplace(id, std::nullopt); });
        }

        auto TreeDFS::explore() -> std::vector<TreeNode>
        {
            // Using the number of edges as an upper bound on the number of split nodes inserted
            auto path = std::vector<TreeNode>(
                m_pbs.graph().number_of_edges() + m_pbs.graph().number_of_nodes()
            );
            auto [out, _] = visit_node(m_pbs.root_node(), path.begin());
            path.resize(static_cast<std::size_t>(out - path.begin()));
            return path;
        }

        auto TreeDFS::node_not_installable(node_id id) -> Status
        {
            auto installables_contains = [&](auto&& lid) { return leaf_installables.contains(lid); };
            const auto& conflicts = m_pbs.conflicts();

            // Conflicts are tricky to handle because they are not an isolated problem, they only
            // appear in conjunction with another leaf.
            // The first time we see a conflict, we add it to the "installables" and return true.
            // Otherwise (if the conflict contains a node in conflict with a node that is
            // "installable"), we return false.
            if (conflicts.has_conflict(id))
            {
                const auto& conflict_with = conflicts.conflicts(id);
                if (std::any_of(conflict_with.begin(), conflict_with.end(), installables_contains))
                {
                    return true;
                }
                leaf_installables.insert(id);
                return false;
            }

            // Assuming any other type of leave is a kind of problem and other nodes not.
            return m_pbs.graph().successors(id).size() == 0;
        }

        auto TreeDFS::node_type(node_id id) const -> TreeNode::Type
        {
            const bool has_predecessors = m_pbs.graph().predecessors(id).size() > 0;
            const bool has_successors = m_pbs.graph().successors(id).size() > 0;
            const bool is_visited = m_node_visited.at(id).has_value();
            // We purposefully check if the node is a leaf before checking if it
            // is visited because showing a single  node again is more intelligible than
            // refering to another one.
            if (!has_successors)
            {
                return TreeNode::Type::leaf;
            }
            else if (is_visited)
            {
                return TreeNode::Type::visited;
            }
            else
            {
                return has_predecessors ? TreeNode::Type::diving : TreeNode::Type::root;
            }
        }

        auto TreeDFS::successors_per_dep(node_id from, bool name_only)
        {
            // The key are sorted by alphabetical order of the dependency name
            auto out = std::map<std::string, std::vector<node_id>>();
            for (auto to : m_pbs.graph().successors(from))
            {
                const auto& edge = m_pbs.graph().edge(from, to);
                std::string key = edge.name();
                if (!name_only)
                {
                    // Making up an arbitrary string represnetation of the edge
                    key += edge.versions_and_build_strings_trunc(
                                   "",
                                   "",
                                   std::numeric_limits<std::size_t>::max()
                    )
                               .first;
                }
                out[key].push_back(to);
            }
            return out;
        }

        /**
         * Specific concatenation for a const vector and a value.
         */
        template <typename T, typename U>
        auto concat(const std::vector<T>& v, U&& x) -> std::vector<T>
        {
            auto out = std::vector<T>();
            out.reserve(v.size() + 1);
            out.insert(out.begin(), v.begin(), v.end());
            out.emplace_back(std::forward<U>(x));
            return out;
        }

        auto TreeDFS::visit_split(
            const std::vector<node_id>& children_ids,
            SiblingNumber position,
            const TreeNode& from,
            TreeNodeIter out
        ) -> std::pair<TreeNodeIter, Status>
        {
            auto& ongoing = *(out++);
            // There is no single node_id for this dynamically created node, we use the vector
            // to represent all nodes.
            assert(children_ids.size() > 0);
            ongoing = TreeNode{
                /* .ancestry= */ concat(from.ancestry, position),
                /* .ids= */ children_ids,
                /* .ids_from= */ from.ids,
                /* .type= */ TreeNode::Type::split,
                /* .type_from= */ from.type,
                /* .status= */ false,  // Placeholder updated
            };

            const TreeNodeIter children_begin = out;
            // TODO(C++20) an enumerate view ``views::zip(views::iota(), children_ids)``
            const std::size_t n_children = children_ids.size();
            for (std::size_t i = 0; i < n_children; ++i)
            {
                const bool last = (i == n_children - 1);
                const auto child_pos = last ? SiblingNumber::last : SiblingNumber::not_last;
                Status status;
                std::tie(out, status) = visit_node(children_ids[i], child_pos, ongoing, out);
                // If there are any valid option in the split, the split is iself valid.
                ongoing.status |= status;
            }

            // All children are the same type of visited or leaves, no grand-children,
            // and same status.
            // We dynamically delete all children and mark the whole node as such.
            auto all_same_split_children = [](TreeNodeIter first, TreeNodeIter last) -> bool
            {
                if (last <= first)
                {
                    return true;
                }
                auto same = [&first](TreeNode const& tn)
                { return (tn.type == first->type) && (tn.status == first->status); };
                return std::all_of(first, last, same);
            };
            const TreeNodeIter children_end = out;
            if ((n_children >= 1) && all_same_split_children(children_begin, children_end))
            {
                ongoing.type = children_begin->type;
                out = children_begin;
            }

            return { out, ongoing.status };
        }

        auto TreeDFS::visit_node(node_id root_id, TreeNodeIter out) -> std::pair<TreeNodeIter, Status>
        {
            auto& ongoing = *(out++);
            ongoing = TreeNode{
                /* .ancestry= */ {},
                /* .ids= */ { root_id },
                /* .ids_from= */ { root_id },
                /* .type= */ node_type(root_id),
                /* .type_from= */ node_type(root_id),
                /* .status= */ {},  // Placeholder updated
            };

            auto out_status = visit_node_impl(root_id, ongoing, out);
            ongoing.status = out_status.second;
            return out_status;
        }

        auto
        TreeDFS::visit_node(node_id id, SiblingNumber position, const TreeNode& from, TreeNodeIter out)
            -> std::pair<TreeNodeIter, Status>
        {
            auto& ongoing = *(out++);
            ongoing = TreeNode{
                /* .ancestry= */ concat(from.ancestry, position),
                /* .ids= */ { id },
                /* .ids_from= */ from.ids,
                /* .type= */ node_type(id),
                /* .type_from= */ from.type,
                /* .status= */ {},  // Placeholder updated
            };

            auto out_status = visit_node_impl(id, ongoing, out);
            ongoing.status = out_status.second;
            return out_status;
        }

        auto TreeDFS::visit_node_impl(node_id id, const TreeNode& ongoing, TreeNodeIter out)
            -> std::pair<TreeNodeIter, Status>
        {
            // At depth 0, we use a stric grouping of edges to avoid gathering user requirements
            // that have the same name (e.g. a mistake "python=3.7" "python=3.8").
            const auto successors = successors_per_dep(id, ongoing.depth() > 0);

            if (const auto status = m_node_visited[id]; status.has_value())
            {
                return { out, status.value() };
            }

            Status status = true;
            // TODO(C++20) an enumerate view ``views::zip(views::iota(), children_ids)``
            std::size_t i = 0;
            for (const auto& [_, children] : successors)
            {
                const auto children_pos = i == successors.size() - 1 ? SiblingNumber::last
                                                                     : SiblingNumber::not_last;
                Status child_status = true;
                assert(children.size() > 0);
                if (children.size() > 1)
                {
                    std::tie(out, child_status) = visit_split(children, children_pos, ongoing, out);
                }
                else
                {
                    std::tie(out, child_status) = visit_node(children[0], children_pos, ongoing, out);
                }
                // All children statuses need to be valid for a parent to be valid.
                status &= child_status;
                ++i;
            }

            // Node installability status is checked *after* visiting the children because there
            // are cases where both non-leaf nodes and their children have conflicts so we need
            // to visit children to exaplin conflicts.
            // In most cases though, only leaves would have conflicts and the loop above would be
            // empty in such case.
            // Warning node_not_installable has side effects and must be called unconditionally
            // (first here).
            status = !node_not_installable(id) && status;

            m_node_visited[id] = status;
            return { out, status };
        }

        /**
         * Transform a path generated by ``TreeDFS`` into a string representation.
         */
        class TreeExplainer
        {
        public:

            static auto explain(
                std::ostream& outs,
                const CompressedProblemsGraph& pbs,
                const ProblemsMessageFormat& format,
                const std::vector<TreeNode>& path
            ) -> std::ostream&;

        private:

            using Status = TreeNode::Status;
            using SiblingNumber = TreeNode::SiblingNumber;
            using node_id = CompressedProblemsGraph::node_id;
            using node_t = CompressedProblemsGraph::node_t;
            using edge_t = CompressedProblemsGraph::edge_t;

            std::ostream& m_outs;
            const CompressedProblemsGraph& m_pbs;
            const ProblemsMessageFormat& m_format;

            TreeExplainer(
                std::ostream& outs,
                const CompressedProblemsGraph& pbs,
                const ProblemsMessageFormat& format
            );

            template <typename... Args>
            void write(Args&&... args);
            void write_ancestry(const std::vector<SiblingNumber>& ancestry);
            void write_pkg_list(const TreeNode& tn);
            void write_pkg_dep(const TreeNode& tn);
            void write_pkg_repr(const TreeNode& tn);
            void write_root(const TreeNode& tn);
            void write_diving(const TreeNode& tn);
            void write_split(const TreeNode& tn);
            void write_leaf(const TreeNode& tn);
            void write_visited(const TreeNode& tn);
            void write_path(const std::vector<TreeNode>& path);

            template <typename Node>
            auto concat_nodes_impl(const std::vector<node_id>& ids) -> Node;
            auto concat_nodes(const std::vector<node_id>& ids) -> node_t;
            auto concat_edges(const std::vector<node_id>& from, const std::vector<node_id>& to)
                -> edge_t;
        };

        /*************************************
         *  Implementation of TreeExplainer  *
         *************************************/

        TreeExplainer::TreeExplainer(
            std::ostream& outs,
            const CompressedProblemsGraph& pbs,
            const ProblemsMessageFormat& format
        )
            : m_outs(outs)
            , m_pbs(pbs)
            , m_format(format)
        {
        }

        template <typename... Args>
        void TreeExplainer::write(Args&&... args)
        {
            (m_outs << ... << std::forward<Args>(args));
        }

        void TreeExplainer::write_ancestry(const std::vector<SiblingNumber>& ancestry)
        {
            const std::size_t size = ancestry.size();
            const auto indents = m_format.indents;
            if (size > 0)
            {
                for (std::size_t i = 0; i < size - 1; ++i)
                {
                    write(indents[static_cast<std::size_t>(ancestry[i])]);
                }
                write(indents[2 + static_cast<std::size_t>(ancestry[size - 1])]);
            }
        }

        void TreeExplainer::write_pkg_list(const TreeNode& tn)
        {
            auto do_write = [&](const auto& node)
            {
                using Node = std::remove_cv_t<std::remove_reference_t<decltype(node)>>;
                if constexpr (!std::is_same_v<Node, CompressedProblemsGraph::RootNode>)
                {
                    auto const style = tn.status ? m_format.available : m_format.unavailable;
                    auto [versions_trunc, size] = node.versions_trunc();
                    write(fmt::format(style, (size == 1 ? "{} {}" : "{} [{}]"), node.name(), versions_trunc)
                    );
                }
            };
            std::visit(do_write, concat_nodes(tn.ids));
        }

        void TreeExplainer::write_pkg_dep(const TreeNode& tn)
        {
            auto edges = concat_edges(tn.ids_from, tn.ids);
            const auto style = tn.status ? m_format.available : m_format.unavailable;
            // We show the build string in pkg_dep and not pkg_list because hand written build
            // string are more likely to contain vital information about the variant.
            auto [vers_builds_trunc, size] = edges.versions_and_build_strings_trunc();
            if (util::strip(vers_builds_trunc).empty())
            {
                write(fmt::format(style, "{}", edges.name()));
            }
            else
            {
                write(fmt::format(style, (size == 1 ? "{} {}" : "{} [{}]"), edges.name(), vers_builds_trunc)
                );
            }
        }

        void TreeExplainer::write_pkg_repr(const TreeNode& tn)
        {
            if (tn.ids_from.size() > 1)
            {
                assert(tn.depth() > 1);
                write_pkg_list(tn);
            }
            // Node is virtual, represent multiple nodes (like a split)
            else
            {
                write_pkg_dep(tn);
            }
        }

        void TreeExplainer::write_root(const TreeNode& tn)
        {
            assert(tn.ids.size() == 1);  // The root is always a single node
            if (m_pbs.graph().successors(tn.ids.front()).size() > 1)
            {
                write("The following packages are incompatible");
            }
            else
            {
                write("The following package could not be installed");
            }
        }

        void TreeExplainer::write_diving(const TreeNode& tn)
        {
            write_pkg_repr(tn);
            if (tn.depth() == 1)
            {
                if (tn.status)
                {
                    write(" is installable and it requires");
                }
                else
                {
                    write(" is not installable because it requires");
                }
            }
            else if (tn.type_from == TreeNode::Type::split)
            {
                write(" would require");
            }
            else
            {
                write(", which requires");
            }
        }

        void TreeExplainer::write_split(const TreeNode& tn)
        {
            write_pkg_repr(tn);
            if (tn.status)
            {
                if (tn.depth() == 1)
                {
                    write(" is installable with the potential options");
                }
                else
                {
                    write(" with the potential options");
                }
            }
            else
            {
                if (tn.depth() == 1)
                {
                    write(" is not installable because there are no viable options");
                }
                else
                {
                    write(" but there are no viable options");
                }
            }
        }

        void TreeExplainer::write_leaf(const TreeNode& tn)
        {
            auto do_write = [&](const auto& node)
            {
                using Node = std::remove_cv_t<std::remove_reference_t<decltype(node)>>;
                using RootNode = CompressedProblemsGraph::RootNode;
                using PackageListNode = CompressedProblemsGraph::PackageListNode;
                using UnresolvedDepListNode = CompressedProblemsGraph::UnresolvedDependencyListNode;
                using ConstraintListNode = CompressedProblemsGraph::ConstraintListNode;

                if constexpr (std::is_same_v<Node, RootNode>)
                {
                    assert(false);
                }
                else if constexpr (std::is_same_v<Node, PackageListNode> || std::is_same_v<Node, ConstraintListNode>)
                {
                    write_pkg_repr(tn);
                    if (tn.status)
                    {
                        if (tn.depth() == 1)
                        {
                            write(" is requested and can be installed");
                        }
                        else
                        {
                            write(", which can be installed");
                        }
                    }
                    else
                    {
                        // Assuming this is always a conflict.
                        if (tn.depth() == 1)
                        {
                            write(" is not installable because it");
                        }
                        else if (tn.type_from != TreeNode::Type::split)
                        {
                            write(", which");
                        }
                        write(" conflicts with any installable versions previously reported");
                    }
                }
                else if constexpr (std::is_same_v<Node, UnresolvedDepListNode>)
                {
                    write_pkg_repr(tn);
                    if ((tn.depth() > 1) && (tn.type_from != TreeNode::Type::split))
                    {
                        write(", which");
                    }
                    // Virtual package
                    if (util::starts_with(node.name(), "__"))
                    {
                        write(" is missing on the system");
                    }
                    else
                    {
                        write(
                            " does not exist (perhaps ",
                            (tn.depth() == 1 ? "a typo or a " : "a "),
                            "missing channel)"
                        );
                    }
                }
            };
            std::visit(do_write, concat_nodes(tn.ids));
        }

        void TreeExplainer::write_visited(const TreeNode& tn)
        {
            write_pkg_repr(tn);
            if (tn.status)
            {
                write(", which can be installed (as previously explained)");
            }
            else
            {
                write(", which cannot be installed (as previously explained)");
            }
        }

        void TreeExplainer::write_path(const std::vector<TreeNode>& path)
        {
            const std::size_t length = path.size();
            for (std::size_t i = 0; i < length; ++i)
            {
                const bool last = (i == length - 1);
                const auto& tn = path[i];
                write_ancestry(tn.ancestry);
                switch (tn.type)
                {
                    case (TreeNode::Type::root):
                    {
                        write_root(tn);
                        write(last ? "." : "\n");
                        break;
                    }
                    case (TreeNode::Type::diving):
                    {
                        write_diving(tn);
                        write(last ? "." : "\n");
                        break;
                    }
                    case (TreeNode::Type::split):
                    {
                        write_split(tn);
                        write(last ? "." : "\n");
                        break;
                    }
                    case (TreeNode::Type::leaf):
                    {
                        write_leaf(tn);
                        write(last ? "." : ";\n");
                        break;
                    }
                    case (TreeNode::Type::visited):
                    {
                        write_visited(tn);
                        write(last ? "." : ";\n");
                        break;
                    }
                }
            }
        }

        auto TreeExplainer::explain(
            std::ostream& outs,
            const CompressedProblemsGraph& pbs,
            const ProblemsMessageFormat& format,
            const std::vector<TreeNode>& path
        ) -> std::ostream&
        {
            auto explainer = TreeExplainer(outs, pbs, format);
            explainer.write_path(path);
            return outs;
        }

        template <typename Node>
        auto TreeExplainer::concat_nodes_impl(const std::vector<node_id>& ids) -> Node
        {
            Node out = {};
            for (auto id : ids)
            {
                const auto& node = std::get<Node>(m_pbs.graph().node(id));
                out.insert(node.begin(), node.end());
            }
            return out;
        }

        auto TreeExplainer::concat_nodes(const std::vector<node_id>& ids) -> node_t
        {
            assert(ids.size() > 0);
            assert(std::all_of(
                ids.begin(),
                ids.end(),
                [&](auto id)
                { return m_pbs.graph().node(ids.front()).index() == m_pbs.graph().node(id).index(); }
            ));

            return std::visit(
                [&](const auto& node) -> node_t
                {
                    using Node = std::remove_cv_t<std::remove_reference_t<decltype(node)>>;
                    if constexpr (std::is_same_v<Node, CompressedProblemsGraph::RootNode>)
                    {
                        return CompressedProblemsGraph::RootNode();
                    }
                    else
                    {
                        return concat_nodes_impl<Node>(ids);
                    }
                },
                m_pbs.graph().node(ids.front())
            );
        }

        auto
        TreeExplainer::concat_edges(const std::vector<node_id>& from, const std::vector<node_id>& to)
            -> edge_t
        {
            auto out = edge_t{};
            for (auto f : from)
            {
                for (auto t : to)
                {
                    const auto& e = m_pbs.graph().edge(f, t);
                    out.insert(e.begin(), e.end());
                }
            }
            return out;
        }
    }

    std::ostream& print_problem_tree_msg(
        std::ostream& out,
        const CompressedProblemsGraph& pbs,
        const ProblemsMessageFormat& format
    )
    {
        auto dfs = TreeDFS(pbs);
        auto path = dfs.explore();
        TreeExplainer::explain(out, pbs, format, path);
        return out;
    }

    std::string
    problem_tree_msg(const CompressedProblemsGraph& pbs, const ProblemsMessageFormat& format)
    {
        std::stringstream ss;
        print_problem_tree_msg(ss, pbs, format);
        return ss.str();
    }
}
