// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_C_API_H
#define MAMBA_API_C_API_H

#ifdef __cplusplus
extern "C"
{
#endif

    int mamba_create();

    int mamba_install();

    int mamba_update(int update_all = 0);

    int mamba_remove(int remove_all = 0);

    int mamba_list(const char* regex = "");

    int mamba_info();

    int mamba_config_list();

    int mamba_set_cli_config(const char* name, const char* value);

    int mamba_set_config(const char* name, const char* value);

    int mamba_clear_config(const char* name);

    int mamba_use_conda_root_prefix(int force = 0);

#ifdef __cplusplus
}
#endif

#endif
