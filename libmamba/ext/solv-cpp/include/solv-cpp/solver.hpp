// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_SOLVER_HPP
#define MAMBA_SOLV_SOLVER_HPP

#include <memory>
#include <optional>
#include <string>

// START Only required for broken header <solv/rule.h>
#include <solv/poolid.h>
extern "C"
{
    typedef struct s_Solvable Solvable;
    typedef struct s_Map Map;
    typedef struct s_Queue Queue;
}
// END
#include <solv/rules.h>

#include "solv-cpp/ids.hpp"
#include "solv-cpp/queue.hpp"

extern "C"
{
    using Solver = struct s_Solver;
}

namespace solv
{
    class ObjPool;
    class ObjQueue;

    auto enum_name(::SolverRuleinfo e) -> std::string_view;

    struct ObjRuleInfo
    {
        std::optional<SolvableId> from_id;
        std::optional<SolvableId> to_id;
        std::optional<DependencyId> dep_id;
        ::SolverRuleinfo type;
        ::SolverRuleinfo klass;
    };

    class ObjSolver
    {
    public:

        ObjSolver(const ObjPool& pool);
        ~ObjSolver();

        [[nodiscard]] auto raw() -> ::Solver*;
        [[nodiscard]] auto raw() const -> const ::Solver*;

        void set_flag(SolverFlag flag, bool value);
        [[nodiscard]] auto get_flag(SolverFlag flag) const -> bool;

        [[nodiscard]] auto solve(const ObjPool& pool, const ObjQueue& jobs) -> bool;

        [[nodiscard]] auto problem_count() const -> std::size_t;
        [[nodiscard]] auto problem_to_string(const ObjPool& pool, ProblemId id) const -> std::string;
        template <typename UnaryFunc>
        void for_each_problem_id(UnaryFunc&& func) const;

        /**
         * Return an @ref ObjQueue of @ref RuleId with all rules involved in a current problem.
         */
        [[nodiscard]] auto problem_rules(ProblemId id) const -> ObjQueue;
        [[nodiscard]] auto get_rule_info(const ObjPool& pool, RuleId id) const -> ObjRuleInfo;
        [[nodiscard]] auto
        rule_info_to_string(const ObjPool& pool, const ObjRuleInfo& id) const -> std::string;

    private:

        struct SolverDeleter
        {
            void operator()(::Solver* ptr);
        };

        std::unique_ptr<::Solver, ObjSolver::SolverDeleter> m_solver = nullptr;

        [[nodiscard]] auto next_problem(ProblemId id = 0) const -> ProblemId;
    };

    /*********************************
     *  Implementation of ObjSolver  *
     *********************************/

    template <typename UnaryFunc>
    void ObjSolver::for_each_problem_id(UnaryFunc&& func) const
    {
        for (ProblemId id = next_problem(); id != 0; id = next_problem(id))
        {
            func(id);
        }
    }

}
#endif
