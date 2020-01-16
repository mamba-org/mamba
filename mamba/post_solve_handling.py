# -*- coding: utf-8 -*-
# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

from conda.base.constants import DepsModifier, UpdateModifier
from conda._vendor.boltons.setutils import IndexedSet
from conda.core.prefix_data import PrefixData
from conda.models.prefix_graph import PrefixGraph
from conda._vendor.toolz import concatv
from conda.models.match_spec import MatchSpec

def post_solve_handling(context, prefix_data, final_precs, specs_to_add, specs_to_remove):
    # Special case handling for various DepsModifier flags.
    if context.deps_modifier == DepsModifier.NO_DEPS:
        # In the NO_DEPS case, we need to start with the original list of packages in the
        # environment, and then only modify packages that match specs_to_add or
        # specs_to_remove.
        #
        # Help information notes that use of NO_DEPS is expected to lead to broken
        # environments.
        _no_deps_solution = IndexedSet(prefix_data.iter_records())
        only_remove_these = set(prec
                                for spec in specs_to_remove
                                for prec in _no_deps_solution
                                if spec.match(prec))
        _no_deps_solution -= only_remove_these

        only_add_these = set(prec
                             for spec in specs_to_add
                             for prec in final_precs
                             if spec.match(prec))
        remove_before_adding_back = set(prec.name for prec in only_add_these)
        _no_deps_solution = IndexedSet(prec for prec in _no_deps_solution
                                       if prec.name not in remove_before_adding_back)
        _no_deps_solution |= only_add_these
        # ssc.solution_precs = _no_deps_solution
        solution_precs = _no_deps_solution

        return solution_precs, specs_to_add, specs_to_remove
    # TODO: check if solution is satisfiable, and emit warning if it's not

    elif (context.deps_modifier == DepsModifier.ONLY_DEPS
            and context.update_modifier != UpdateModifier.UPDATE_DEPS):
        # Using a special instance of PrefixGraph to remove youngest child nodes that match
        # the original specs_to_add.  It's important to remove only the *youngest* child nodes,
        # because a typical use might be `conda install --only-deps python=2 flask`, and in
        # that case we'd want to keep python.
        #
        # What are we supposed to do if flask was already in the environment?
        # We can't be removing stuff here that's already in the environment.
        # 
        # What should be recorded for the user-requested specs in this case? Probably all
        # direct dependencies of flask.
        graph = PrefixGraph(final_precs, specs_to_add)
        removed_nodes = graph.remove_youngest_descendant_nodes_with_specs()
        specs_to_add = set(specs_to_add)
        specs_to_add_names = set((s.name for s in specs_to_add))

        for prec in removed_nodes:
            for dep in prec.depends:
                dep = MatchSpec(dep)
                if dep.name not in specs_to_add_names:
                    specs_to_add.add(dep)
        # unfreeze
        specs_to_add = frozenset(specs_to_add)

        # Add back packages that are already in the prefix.
        specs_to_remove_names = set(spec.name for spec in specs_to_remove)
        add_back = tuple(prefix_data.get(node.name, None) for node in removed_nodes
                         if node.name not in specs_to_remove_names)
        solution_precs = tuple(
            PrefixGraph(concatv(graph.graph, filter(None, add_back))).graph
        )

        return solution_precs, specs_to_add, specs_to_remove

    return final_precs, specs_to_add, specs_to_remove

    #     # TODO: check if solution is satisfiable, and emit warning if it's not

    # elif ssc.update_modifier == UpdateModifier.UPDATE_DEPS:
    #     # Here we have to SAT solve again :(  It's only now that we know the dependency
    #     # chain of specs_to_add.
    #     #
    #     # UPDATE_DEPS is effectively making each spec in the dependency chain a user-requested
    #     # spec.  We don't modify pinned_specs, track_features_specs, or specs_to_add.  For
    #     # all other specs, we drop all information but name, drop target, and add them to
    #     # the specs_to_add that gets recorded in the history file.
    #     #
    #     # It's like UPDATE_ALL, but only for certain dependency chains.
    #     graph = PrefixGraph(ssc.solution_precs)
    #     update_names = set()
    #     for spec in specs_to_add:
    #         node = graph.get_node_by_name(spec.name)
    #         update_names.update(ancest_rec.name for ancest_rec in graph.all_ancestors(node))
    #     specs_map = {name: MatchSpec(name) for name in update_names}

    #     # Remove pinned_specs and any python spec (due to major-minor pinning business rule).
    #     # Add in the original specs_to_add on top.
    #     for spec in ssc.pinned_specs:
    #         specs_map.pop(spec.name, None)
    #     if "python" in specs_map:
    #         python_rec = prefix_data.get("python")
    #         py_ver = ".".join(python_rec.version.split(".")[:2]) + ".*"
    #         specs_map["python"] = MatchSpec(name="python", version=py_ver)
    #     specs_map.update({spec.name: spec for spec in specs_to_add})
    #     new_specs_to_add = tuple(itervalues(specs_map))

    #     # It feels wrong/unsafe to modify this instance, but I guess let's go with it for now.
    #     specs_to_add = new_specs_to_add
    #     ssc.solution_precs = self.solve_final_state(
    #         update_modifier=UpdateModifier.UPDATE_SPECS,
    #         deps_modifier=ssc.deps_modifier,
    #         prune=ssc.prune,
    #         ignore_pinned=ssc.ignore_pinned,
    #         force_remove=ssc.force_remove
    #     )
    #     ssc.prune = False

    # if ssc.prune:
    #     graph = PrefixGraph(ssc.solution_precs, final_environment_specs)
    #     graph.prune()
    #     ssc.solution_precs = tuple(graph.graph)

    # return ssc

