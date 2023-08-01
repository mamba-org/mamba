// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBATESTS_HPP
#define LIBMAMBATESTS_HPP

#include "mamba/core/context.hpp"

namespace mambatests
{
    // Provides the context object to use in all tests needing it.
    // Note that this context is setup to handle logigng and signal handling.
    inline mamba::Context& context()
    {
        // FIXME: once Console::instance() is removed, reactivate this code:
        // static mamba::Context ctx{{
        //     /* .enable_logging_and_signal_handling  = */ true
        // }};
        // return ctx;
        return mamba::Context::instance();
    }

}


#endif
