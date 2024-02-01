// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_IDS_HPP
#define MAMBA_SOLV_IDS_HPP

#include <solv/pooltypes.h>

namespace mamba::solv
{
    using StringId = ::Id;
    using DependencyId = ::Id;
    using RepoId = ::Id;
    using SolvableId = ::Id;
    using OffsetId = ::Id;
    using RuleId = ::Id;
    using ProblemId = ::Id;
    using DependencyMarker = ::Id;
    using KeyNameId = ::Id;

    using RelationFlag = int;
    using DistType = int;
    using SolverFlag = int;
    using TransactionOrderFlag = int;
    using TransactionStepType = int;
    using TransactionMode = int;

    enum struct LoopControl
    {
        Continue,
        Break
    };
}

#endif
