// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBATESTS_HPP
#define LIBMAMBATESTS_HPP

#include "mamba/core/context.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"

namespace mambatests
{
    struct Singletons
    {
        // mamba::MainExecutor main_executor; // FIXME: reactivate once the tests are not indirectly
        // using this anymore
        mamba::Context context{ { /* .enable_logging_and_signal_handling = */ true } };
        mamba::Console console{ context };
    };

    inline Singletons& singletons()  // FIXME: instead of this create the objects in main so that
                                     // their lifetime doesnt go beyond main() scope.
    {
        static Singletons singletons;
        return singletons;
    }

    // Provides the context object to use in all tests needing it.
    // Note that this context is setup to handle logigng and signal handling.
    inline mamba::Context& context()
    {
        return singletons().context;
    }

}


#endif
