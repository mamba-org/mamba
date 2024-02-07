// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>

#include <solv/poolid.h>
#include <solv/solvable.h>
#include <solv/solver.h>
// broken headers go last
#include <solv/problems.h>
#include <solv/rules.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/solver.hpp"

namespace mamba::solv
{
    auto enum_name(::SolverRuleinfo rule) -> std::string_view
    {
        switch (rule)
        {
            case (SOLVER_RULE_UNKNOWN):
            {
                return "SOLVER_RULE_UNKNOWN";
            }
            case (SOLVER_RULE_PKG):
            {
                return "SOLVER_RULE_PKG";
            }
            case (SOLVER_RULE_PKG_NOT_INSTALLABLE):
            {
                return "SOLVER_RULE_PKG_NOT_INSTALLABLE";
            }
            case (SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP):
            {
                return "SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP";
            }
            case (SOLVER_RULE_PKG_REQUIRES):
            {
                return "SOLVER_RULE_PKG_REQUIRES";
            }
            case (SOLVER_RULE_PKG_SELF_CONFLICT):
            {
                return "SOLVER_RULE_PKG_SELF_CONFLICT";
            }
            case (SOLVER_RULE_PKG_CONFLICTS):
            {
                return "SOLVER_RULE_PKG_CONFLICTS";
            }
            case (SOLVER_RULE_PKG_SAME_NAME):
            {
                return "SOLVER_RULE_PKG_SAME_NAME";
            }
            case (SOLVER_RULE_PKG_OBSOLETES):
            {
                return "SOLVER_RULE_PKG_OBSOLETES";
            }
            case (SOLVER_RULE_PKG_IMPLICIT_OBSOLETES):
            {
                return "SOLVER_RULE_PKG_IMPLICIT_OBSOLETES";
            }
            case (SOLVER_RULE_PKG_INSTALLED_OBSOLETES):
            {
                return "SOLVER_RULE_PKG_INSTALLED_OBSOLETES";
            }
            case (SOLVER_RULE_PKG_RECOMMENDS):
            {
                return "SOLVER_RULE_PKG_RECOMMENDS";
            }
            case (SOLVER_RULE_PKG_CONSTRAINS):
            {
                return "SOLVER_RULE_PKG_CONSTRAINS";
            }
            case (SOLVER_RULE_UPDATE):
            {
                return "SOLVER_RULE_UPDATE";
            }
            case (SOLVER_RULE_FEATURE):
            {
                return "SOLVER_RULE_FEATURE";
            }
            case (SOLVER_RULE_JOB):
            {
                return "SOLVER_RULE_JOB";
            }
            case (SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP):
            {
                return "SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP";
            }
            case (SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM):
            {
                return "SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM";
            }
            case (SOLVER_RULE_JOB_UNKNOWN_PACKAGE):
            {
                return "SOLVER_RULE_JOB_UNKNOWN_PACKAGE";
            }
            case (SOLVER_RULE_JOB_UNSUPPORTED):
            {
                return "SOLVER_RULE_JOB_UNSUPPORTED";
            }
            case (SOLVER_RULE_DISTUPGRADE):
            {
                return "SOLVER_RULE_DISTUPGRADE";
            }
            case (SOLVER_RULE_INFARCH):
            {
                return "SOLVER_RULE_INFARCH";
            }
            case (SOLVER_RULE_CHOICE):
            {
                return "SOLVER_RULE_CHOICE";
            }
            case (SOLVER_RULE_LEARNT):
            {
                return "SOLVER_RULE_LEARNT";
            }
            case (SOLVER_RULE_BEST):
            {
                return "SOLVER_RULE_BEST";
            }
            case (SOLVER_RULE_YUMOBS):
            {
                return "SOLVER_RULE_YUMOBS";
            }
            case (SOLVER_RULE_RECOMMENDS):
            {
                return "SOLVER_RULE_RECOMMENDS";
            }
            case (SOLVER_RULE_BLACK):
            {
                return "SOLVER_RULE_BLACK";
            }
            case (SOLVER_RULE_STRICT_REPO_PRIORITY):
            {
                return "SOLVER_RULE_STRICT_REPO_PRIORITY";
            }
            default:
            {
                throw std::runtime_error("Invalid SolverRuleinfo: " + std::to_string(rule));
            }
        }
    }

    void ObjSolver::SolverDeleter::operator()(::Solver* ptr)
    {
        ::solver_free(ptr);
    }

    ObjSolver::ObjSolver(const ObjPool& pool)
        : m_solver(::solver_create(const_cast<::Pool*>(pool.raw())))
    {
    }

    ObjSolver::~ObjSolver() = default;

    auto ObjSolver::raw() -> ::Solver*
    {
        return m_solver.get();
    }

    auto ObjSolver::raw() const -> const ::Solver*
    {
        return m_solver.get();
    }

    void ObjSolver::set_flag(SolverFlag flag, bool value)
    {
        ::solver_set_flag(raw(), flag, value);
    }

    auto ObjSolver::get_flag(SolverFlag flag) const -> bool
    {
        const auto val = ::solver_get_flag(const_cast<::Solver*>(raw()), flag);
        assert((val == 0) || (val == 1));
        return val != 0;
    }

    auto ObjSolver::solve(const ObjPool& /* pool */, const ObjQueue& jobs) -> bool
    {
        // pool is captured inside solver so we take it as a parameter to be explicit.
        const auto n_pbs = ::solver_solve(raw(), const_cast<::Queue*>(jobs.raw()));
        return n_pbs == 0;
    }

    auto ObjSolver::problem_count() const -> std::size_t
    {
        return ::solver_problem_count(const_cast<::Solver*>(raw()));
    }

    auto ObjSolver::problem_to_string(const ObjPool& /* pool */, ProblemId id) const -> std::string
    {
        // pool is captured inside solver so we take it as a parameter to be explicit.
        return ::solver_problem2str(const_cast<::Solver*>(raw()), id);
    }

    auto ObjSolver::next_problem(ProblemId id) const -> ProblemId
    {
        return ::solver_next_problem(const_cast<::Solver*>(raw()), id);
    }

    auto ObjSolver::problem_rules(ProblemId id) const -> ObjQueue
    {
        ObjQueue rules = {};
        ::solver_findallproblemrules(const_cast<::Solver*>(raw()), id, rules.raw());
        return rules;
    }

    auto ObjSolver::get_rule_info(const ObjPool& /* pool */, RuleId id) const -> ObjRuleInfo
    {
        // pool is captured inside solver so we take it as a parameter to be explicit.
        SolvableId from_id = 0;
        SolvableId to_id = 0;
        DependencyId dep_id = 0;
        const auto type = ::solver_ruleinfo(const_cast<::Solver*>(raw()), id, &from_id, &to_id, &dep_id);

        return {
            /* .from_id= */ (from_id != 0) ? std::optional{ from_id } : std::nullopt,
            /* .to_id= */ (to_id != 0) ? std::optional{ to_id } : std::nullopt,
            /* .dep_id= */ (dep_id != 0) ? std::optional{ dep_id } : std::nullopt,
            /* .type= */ type,
            /* .klass= */ ::solver_ruleclass(const_cast<::Solver*>(raw()), id),
        };
    }

    auto ObjSolver::rule_info_to_string(const ObjPool& /* pool */, const ObjRuleInfo& ri) const
        -> std::string
    {
        // pool is captured inside solver so we take it as a parameter to be explicit.
        return ::solver_ruleinfo2str(
            const_cast<::Solver*>(raw()),
            ri.type,
            ri.from_id.value_or(0),
            ri.to_id.value_or(0),
            ri.dep_id.value_or(0)
        );
    }

}
