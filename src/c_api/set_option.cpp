// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/c_api/mambac.h"
#include "mamba/context.hpp"


using namespace mamba;


void
set_opt(int option, int value)
{
    auto& ctx = Context::instance();

    switch (option)
    {
        case MAMBA_USE_INDEX_CACHE:
            ctx.use_index_cache = value;
            break;
        case MAMBA_OFFLINE:
            ctx.offline = value;
            break;
        case MAMBA_QUIET:
            ctx.quiet = value;
            break;
        case MAMBA_JSON:
            ctx.json = value;
            break;
        case MAMBA_AUTO_ACTIVATE_BASE:
            ctx.auto_activate_base = value;
            break;
        case MAMBA_DEV:
            ctx.dev = value;
            break;
        case MAMBA_ON_CI:
            ctx.on_ci = value;
            break;
        case MAMBA_NO_PROGRESS_BARS:
            ctx.no_progress_bars = value;
            break;
        case MAMBA_DRY_RUN:
            ctx.dry_run = value;
            break;
        case MAMBA_ALWAYS_YES:
            ctx.always_yes = value;
            break;
        case MAMBA_KEEP_TEMP_FILES:
            ctx.keep_temp_files = value;
            break;
        case MAMBA_KEEP_TEMP_DIRECTORIES:
            ctx.keep_temp_directories = value;
            break;
        case MAMBA_CHANGE_PS1:
            ctx.change_ps1 = value;
            break;
        case MAMBA_ADD_PIP_AS_PYTHON_DEPENDENCY:
            ctx.add_pip_as_python_dependency = value;
            break;
    }
}
