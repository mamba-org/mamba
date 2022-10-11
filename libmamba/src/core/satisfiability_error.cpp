// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <vector>
#include <algorithm>
#include <map>
#include <type_traits>

#include <solv/pool.h>

#include "mamba/core/output.hpp"
#include "mamba/core/satisfiability_error.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/pool.hpp"

namespace mamba
{

    /**************************************
     *  Implementation of DependencyInfo  *
     **************************************/

    DependencyInfo::DependencyInfo(const std::string& dep)
    {
        static std::regex const regexp("\\s*(\\w[\\w-]*)\\s*([^\\s]*)(?:\\s+([^\\s]+))?\\s*");
        std::smatch matches;
        bool const matched = std::regex_match(dep, matches, regexp);
        // First match is the whole regex match
        if (!matched || matches.size() != 4)
        {
            throw std::runtime_error("Invalid dependency info: " + dep);
        }
        m_name = matches.str(1);
        m_version_range = matches.str(2);
        m_build_range = matches.str(3);
    }

    std::string const& DependencyInfo::name() const
    {
        return m_name;
    }

    std::string const& DependencyInfo::version() const
    {
        return m_version_range;
    }

    std::string const& DependencyInfo::build_string() const
    {
        return m_build_range;
    }

    std::string DependencyInfo::str() const
    {
        std::string out(m_name);
        out.reserve(m_name.size() + (m_version_range.empty() ? 0 : 1) + m_version_range.size()
                    + (m_build_range.empty() ? 0 : 1) + m_version_range.size());
        if (!m_version_range.empty())
        {
            out += ' ';
            out += m_version_range;
        }
        if (!m_build_range.empty())
        {
            out += ' ';
            out += m_build_range;
        }
        return out;
    }

    /*************************************
     *  Implementation of ProblemsGraph  *
     *************************************/

    namespace
    {

        void warn_unexpected_problem(MSolverProblem const& problem)
        {
            // TODO: Once the new error message are not experimental, we should consider
            // reducing this level since it is not somethig the user has control over.
            LOG_WARNING << "Unexpected empty optionals for problem type "
                        << solver_ruleinfo_name(problem.type);
        }

        class ProblemsGraphCreator
        {
        public:
            using SolvId = Id;  // Unscoped from libsolv

            using graph_t = ProblemsGraph::graph_t;
            using RootNode = ProblemsGraph::RootNode;
            using PackageNode = ProblemsGraph::PackageNode;
            using UnresolvedDependencyNode = ProblemsGraph::UnresolvedDependencyNode;
            using ConstraintNode = ProblemsGraph::ConstraintNode;
            using node_t = ProblemsGraph::node_t;
            using node_id = ProblemsGraph::node_id;
            using edge_t = ProblemsGraph::edge_t;
            using conflicts_t = ProblemsGraph::conflicts_t;

            ProblemsGraphCreator(MSolver const& solver, MPool const& pool)
                : m_solver{ solver }
                , m_pool{ pool }
            {
                m_root_node = m_graph.add_node(RootNode());
                parse_problems();
            }

            operator ProblemsGraph() &&
            {
                return { std::move(m_graph), std::move(m_conflicts), m_root_node };
            }

        private:
            MSolver const& m_solver;
            MPool const& m_pool;
            graph_t m_graph;
            conflicts_t m_conflicts;
            std::map<SolvId, node_id> m_solv2node;
            node_id m_root_node;

            /**
             * Add a node and return the node id.
             *
             * If the node is already present and ``update`` is false then the current
             * node is left as it is, otherwise the new value is inserted.
             */
            node_id add_solvable(SolvId solv_id, node_t&& pkg_info, bool update = true);

            void add_conflict(node_id n1, node_id n2);
            [[nodiscard]] bool add_expanded_deps_edges(node_id from_id,
                                                       SolvId dep_id,
                                                       edge_t const& edge);

            void parse_problems();
        };

        auto ProblemsGraphCreator::add_solvable(SolvId solv_id, node_t&& node, bool update)
            -> node_id
        {
            if (auto const iter = m_solv2node.find(solv_id); iter != m_solv2node.end())
            {
                node_id const id = iter->second;
                if (update)
                {
                    m_graph.node(id) = std::move(node);
                }
                return id;
            }
            node_id const id = m_graph.add_node(std::move(node));
            m_solv2node[solv_id] = id;
            return id;
        };

        void ProblemsGraphCreator::add_conflict(node_id n1, node_id n2)
        {
            m_conflicts.add(n1, n2);
        }

        bool ProblemsGraphCreator::add_expanded_deps_edges(node_id from_id,
                                                           SolvId dep_id,
                                                           edge_t const& edge)
        {
            bool added = false;
            for (const auto& solv_id : m_pool.select_solvables(dep_id))
            {
                added = true;
                PackageInfo pkg_info(pool_id2solvable(m_pool, solv_id));
                node_id to_id = add_solvable(
                    solv_id, PackageNode{ std::move(pkg_info), std::nullopt }, false);
                m_graph.add_edge(from_id, to_id, edge);
            }
            return added;
        }

        void ProblemsGraphCreator::parse_problems()
        {
            for (const auto& problem : m_solver.all_problems_structured())
            {
                // These functions do not return a reference so we make sure to
                // compute the value only once.
                std::optional<PackageInfo> source = problem.source();
                std::optional<PackageInfo> target = problem.target();
                std::optional<std::string> dep = problem.dep();
                SolverRuleinfo type = problem.type;

                switch (type)
                {
                    case SOLVER_RULE_PKG_CONSTRAINS:
                    {
                        // A constraint (run_constrained) on source is conflicting with target.
                        // SOLVER_RULE_PKG_CONSTRAINS has a dep, but it can resolve to nothing.
                        // The constraint conflict is actually expressed between the target and
                        // a constrains node child of the source.
                        if (!source || !target || !dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto src_id
                            = add_solvable(problem.source_id,
                                           PackageNode{ std::move(source).value(), std::nullopt });
                        node_id tgt_id = add_solvable(
                            problem.target_id, PackageNode{ std::move(target).value(), { type } });
                        node_id cons_id
                            = add_solvable(problem.dep_id, ConstraintNode{ dep.value() });
                        DependencyInfo edge(dep.value());
                        m_graph.add_edge(src_id, cons_id, std::move(edge));
                        add_conflict(cons_id, tgt_id);
                        break;
                    }
                    case SOLVER_RULE_PKG_REQUIRES:
                    {
                        // Express a dependency on source that is involved in explaining the
                        // problem.
                        // Not all dependency of package will appear, only enough to explain the
                        // problem. It is not a problem in itself, only a part of the graph.
                        if (!dep || !source)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto src_id
                            = add_solvable(problem.source_id,
                                           PackageNode{ std::move(source).value(), std::nullopt });
                        DependencyInfo edge(dep.value());
                        bool added = add_expanded_deps_edges(src_id, problem.dep_id, edge);
                        if (!added)
                        {
                            LOG_WARNING << "Added empty dependency for problem type "
                                        << solver_ruleinfo_name(type);
                        }
                        break;
                    }
                    case SOLVER_RULE_JOB:
                    case SOLVER_RULE_PKG:
                    {
                        // A top level requirement.
                        // The difference between JOB and PKG is unknown (possibly unused).
                        if (!dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        DependencyInfo edge(dep.value());
                        bool added = add_expanded_deps_edges(m_root_node, problem.dep_id, edge);
                        if (!added)
                        {
                            LOG_WARNING << "Added empty dependency for problem type "
                                        << solver_ruleinfo_name(type);
                        }
                        break;
                    }
                    case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
                    case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
                    {
                        // A top level dependency does not exist.
                        // Could be a wrong name or missing channel.
                        if (!dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        DependencyInfo edge(dep.value());
                        node_id tgt_id = add_solvable(
                            problem.target_id, PackageNode{ std::move(dep).value(), { type } });
                        m_graph.add_edge(m_root_node, tgt_id, std::move(edge));
                        break;
                    }
                    case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
                    {
                        // A package dependency does not exist.
                        // Could be a wrong name or missing channel.
                        // This is a partial exaplanation of why a specific solvable (could be any
                        // of the parent) cannot be installed.
                        if (!source || !dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        DependencyInfo edge(dep.value());
                        auto src_id
                            = add_solvable(problem.source_id,
                                           PackageNode{ std::move(source).value(), std::nullopt });
                        node_id tgt_id = add_solvable(
                            problem.target_id, UnresolvedDependencyNode{ std::move(dep).value() });
                        m_graph.add_edge(src_id, tgt_id, std::move(edge));
                        break;
                    }
                    case SOLVER_RULE_PKG_CONFLICTS:
                    case SOLVER_RULE_PKG_SAME_NAME:
                    {
                        // Looking for a valid solution to the installation satisfiability expand to
                        // two solvables of same package that cannot be installed together. This is
                        // a partial exaplanation of why one of the solvables (could be any of the
                        // parent) cannot be installed.
                        if (!source || !target)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        node_id src_id = add_solvable(
                            problem.source_id, PackageNode{ std::move(source).value(), { type } });
                        node_id tgt_id = add_solvable(
                            problem.target_id, PackageNode{ std::move(target).value(), { type } });
                        add_conflict(src_id, tgt_id);
                        break;
                    }
                    case SOLVER_RULE_UPDATE:
                    {
                        // Encounterd in the problems list from libsolv but unknown.
                        // Explicitly ignored until we do something with it.
                        break;
                    }
                    default:
                    {
                        // Many more SolverRuleinfo that heve not been encountered.
                        LOG_WARNING << "Problem type not implemented "
                                    << solver_ruleinfo_name(type);
                        break;
                    }
                }
            };
        }
    }

    auto ProblemsGraph::from_solver(MSolver const& solver, MPool const& pool) -> ProblemsGraph
    {
        return ProblemsGraphCreator(solver, pool);
    }

    ProblemsGraph::ProblemsGraph(graph_t graph, conflicts_t conflicts, node_id root_node)
        : m_graph(std::move(graph))
        , m_conflicts(std::move(conflicts))
        , m_root_node(root_node)
    {
    }

    auto ProblemsGraph::graph() const noexcept -> graph_t const&
    {
        return m_graph;
    }

    auto ProblemsGraph::conflicts() const noexcept -> conflicts_t const&
    {
        return m_conflicts;
    }

    auto ProblemsGraph::root_node() const noexcept -> node_id
    {
        return m_root_node;
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
            std::vector<ProblemsGraph::node_id> const& node_indices, CompFunc&& merge_criteria)
            -> std::vector<old_node_id_list>
        {
            using node_id = ProblemsGraph::node_id;

            std::vector<old_node_id_list> groups{};

            std::size_t const n_nodes = node_indices.size();
            std::vector<bool> node_added_to_a_group(n_nodes, false);
            for (node_id i = 0; i < n_nodes; ++i)
            {
                if (!node_added_to_a_group[i])
                {
                    std::vector<node_id> current_group{};
                    current_group.push_back(node_indices[i]);
                    node_added_to_a_group[i] = true;
                    // This is where we use symetry and transitivity, going through all remaining
                    // nodes and adding them to the current group if they match the criteria.
                    for (node_id j = i + 1; j < n_nodes; ++j)
                    {
                        if ((!node_added_to_a_group[j]) && merge_criteria(i, j))
                        {
                            current_group.push_back(node_indices[j]);
                            node_added_to_a_group[j] = true;
                        }
                    }
                    groups.push_back(std::move(current_group));
                }
            }
            assert(std::all_of(node_added_to_a_group.begin(),
                               node_added_to_a_group.end(),
                               [](auto x) { return x; }));
            return groups;
        }

        /**
         * Used to organize objects by node type (the index of the type in the variant).
         */
        template <typename T>
        using node_type_list = std::vector<T>;

        /**
         * Group indices of variants with same alternative together.
         *
         * If a variant at index ``i`` in the input @p vrnts holds alternative with index ``k``
         * then the output will contain ``i`` in position ``k``.
         */
        template <typename... T>
        auto variant_by_index(std::vector<std::variant<T...>> const& vrnts)
            -> node_type_list<std::vector<std::size_t>>
        {
            auto out = node_type_list<std::vector<std::size_t>>(sizeof...(T));
            auto const n = vrnts.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                out[vrnts[i].index()].push_back(i);
            }
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
        auto merge_node_indices(ProblemsGraph::graph_t::node_list const& nodes,
                                CompFunc&& merge_criteria)
            -> node_type_list<std::vector<old_node_id_list>>
        {
            auto merge_func = [&merge_criteria](auto const& node_indices_of_one_node_type)
            {
                return merge_node_indices_for_one_node_type(node_indices_of_one_node_type,
                                                            std::forward<CompFunc>(merge_criteria));
            };
            auto const nodes_by_type = variant_by_index(nodes);
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
         * ``ranges::transform(f) | ranges::to<std::vector>()``
         */
        template <typename Range, typename Func>
        auto transform_to_vec(Range const& rng, Func&& f)
        {
            using T = typename Range::value_type;
            using O = std::invoke_result_t<Func, T>;
            auto out = std::vector<O>();
            out.reserve(rng.size());
            std::transform(rng.begin(), rng.end(), std::back_inserter(out), std::forward<Func>(f));
            return out;
        }

        /**
         * The criteria for deciding whether to merge two nodes together.
         */
        auto create_merge_criteria(ProblemsGraph const& pbs)
        {
            using node_id = ProblemsGraph::node_id;
            return [&pbs](node_id n1, node_id n2) -> bool
            {
                // TODO needs finetuning, not the same conditions
                return !(pbs.conflicts().in_conflict(n1, n2))
                       && (pbs.graph().successors(n1) == pbs.graph().successors(n2))
                       && (pbs.graph().predecessors(n1) == pbs.graph().predecessors(n2));
            };
        }

        using node_id_mapping = std::vector<CompressedProblemsGraph::node_id>;

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
        void merge_nodes_for_one_node_type(ProblemsGraph::graph_t const& old_graph,
                                           std::vector<old_node_id_list> const& old_groups,
                                           CompressedProblemsGraph::graph_t& new_graph,
                                           node_id_mapping& old_to_new)
        {
            auto get_old_node = [&old_graph](ProblemsGraph::node_id id)
            {
                auto node = old_graph.node(id);
                // Must always be the case that old_groups only references node ids of type Node
                assert(std::holds_alternative<Node>(node));
                return std::get<Node>(std::move(node));
            };

            for (auto const& old_grp : old_groups)
            {
                auto const new_id = new_graph.add_node(transform_to_vec(old_grp, get_old_node));
                for (auto const old_id : old_grp)
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
        auto merge_nodes(ProblemsGraph const& pbs, CompFunc&& merge_criteria)
            -> std::tuple<CompressedProblemsGraph::graph_t,
                          CompressedProblemsGraph::node_id,
                          node_id_mapping>
        {
            auto const& old_graph = pbs.graph();
            auto new_graph = CompressedProblemsGraph::graph_t();
            auto const new_root_node = new_graph.add_node(CompressedProblemsGraph::RootNode());

            auto old_to_new
                = std::vector<CompressedProblemsGraph::node_id>(old_graph.number_of_nodes());
            auto old_ids_groups
                = merge_node_indices(old_graph.nodes(), std::forward<CompFunc>(merge_criteria));

            {
                using Node = ProblemsGraph::RootNode;
                static constexpr auto type_idx = variant_type_index<ProblemsGraph::node_t, Node>();
                assert(old_ids_groups[type_idx].size() == 1);
                assert(old_ids_groups[type_idx][0].size() == 1);
                assert(old_ids_groups[type_idx][0][0] == pbs.root_node());
                old_to_new[pbs.root_node()] = new_root_node;
            }
            {
                using Node = ProblemsGraph::PackageNode;
                static constexpr auto type_idx = variant_type_index<ProblemsGraph::node_t, Node>();
                merge_nodes_for_one_node_type<Node>(
                    old_graph, old_ids_groups[type_idx], new_graph, old_to_new);
            }
            {
                using Node = ProblemsGraph::UnresolvedDependencyNode;
                static constexpr auto type_idx = variant_type_index<ProblemsGraph::node_t, Node>();
                merge_nodes_for_one_node_type<Node>(
                    old_graph, old_ids_groups[type_idx], new_graph, old_to_new);
            }
            {
                using Node = ProblemsGraph::ConstraintNode;
                static constexpr auto type_idx = variant_type_index<ProblemsGraph::node_t, Node>();
                merge_nodes_for_one_node_type<Node>(
                    old_graph, old_ids_groups[type_idx], new_graph, old_to_new);
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
        void merge_edges(ProblemsGraph::graph_t const& old_graph,
                         CompressedProblemsGraph::graph_t& new_graph,
                         node_id_mapping const& old_to_new)
        {
            auto add_new_edge = [&](ProblemsGraph::node_id old_from, ProblemsGraph::node_id old_to)
            {
                auto const new_from = old_to_new[old_from];
                auto const new_to = old_to_new[old_to];
                if (!new_graph.has_edge(new_from, new_to))
                {
                    new_graph.add_edge(new_from, new_to, CompressedProblemsGraph::edge_t());
                }
                new_graph.edge(new_from, new_to).push_back(old_graph.edge(old_from, old_to));
            };
            old_graph.for_each_edge(add_new_edge);
        }

        /**
         * Merge the conflicts according to the new groups of nodes.
         *
         * If two groups contain a node that are respectively in conflicts, then they are in
         * conflicts.
         */
        auto merge_conflicts(ProblemsGraph::conflicts_t const& old_conflicts,
                             node_id_mapping const& old_to_new)
            -> CompressedProblemsGraph::conflicts_t
        {
            auto new_conflicts = CompressedProblemsGraph::conflicts_t();
            for (auto const& [old_from, old_with] : old_conflicts)
            {
                auto const new_from = old_to_new[old_from];
                for (auto const old_to : old_with)
                {
                    new_conflicts.add(new_from, old_to_new[old_to]);
                }
            }
            return new_conflicts;
        }
    }

    // TODO move graph nodes and edges.
    auto CompressedProblemsGraph::from_problems_graph(ProblemsGraph const& pbs)
        -> CompressedProblemsGraph
    {
        auto [graph, root_node, old_to_new] = merge_nodes(pbs, create_merge_criteria(pbs));
        merge_edges(pbs.graph(), graph, old_to_new);
        auto conflicts = merge_conflicts(pbs.conflicts(), old_to_new);
        return { std::move(graph), std::move(conflicts), root_node };
    }

    CompressedProblemsGraph::CompressedProblemsGraph(graph_t graph,
                                                     conflicts_t conflicts,
                                                     node_id root_node)
        : m_graph(std::move(graph))
        , m_conflicts(std::move(conflicts))
        , m_root_node(root_node)
    {
    }

    auto CompressedProblemsGraph::graph() const noexcept -> graph_t const&
    {
        return m_graph;
    }

    auto CompressedProblemsGraph::conflicts() const noexcept -> conflicts_t const&
    {
        return m_conflicts;
    }

    auto CompressedProblemsGraph::root_node() const noexcept -> node_id
    {
        return m_root_node;
    }
}
