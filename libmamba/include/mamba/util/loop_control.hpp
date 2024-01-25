// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_LOOP_CONTROL_HPP
#define MAMBA_UTIL_LOOP_CONTROL_HPP

namespace mamba::util
{
    /**
     * An enum for breaking out of ``for_each`` loops, used as a poor man range.
     */
    enum class LoopControl
    {
        Break,
        Continue,
    };
}
#endif
