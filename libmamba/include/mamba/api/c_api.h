// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_C_API_H
#define MAMBA_API_C_API_H

namespace mamba
{
    class Context;
    class Configuration;
    struct ContextOptions;
    class MainExecutor;
}

#ifdef __cplusplus
extern "C"
{
#endif

    mamba::MainExecutor* mamba_new_main_executor();
    void mamba_delete_main_executor(mamba::MainExecutor* main_executor);

    mamba::Context* mamba_new_context(mamba::ContextOptions* options);
    void mamba_delete_context(mamba::Context* context);

    mamba::Configuration* mamba_new_configuration(mamba::Context* context);
    void mamba_delete_configuration(mamba::Configuration* config);

    int mamba_create(mamba::Configuration* config);

    int mamba_install(mamba::Configuration* config);

    int mamba_update(mamba::Configuration* config, int update_all = 0);

    int mamba_remove(mamba::Configuration* config, int remove_all = 0);

    int mamba_list(mamba::Configuration* config, const char* regex = "");

    int mamba_info(mamba::Configuration* config);

    int mamba_config_list(mamba::Configuration* config);

    int mamba_set_cli_config(mamba::Configuration* config, const char* name, const char* value);

    int mamba_set_config(mamba::Configuration* config, const char* name, const char* value);

    int mamba_clear_config(mamba::Configuration* config, const char* name);

    int mamba_use_conda_root_prefix(mamba::Configuration* config, int force = 0);

#ifdef __cplusplus
}
#endif

#endif
