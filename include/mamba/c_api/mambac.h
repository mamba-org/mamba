// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_MAMBAC_H
#define MAMBA_MAMBAC_H

#ifdef __cplusplus
extern "C"
{
#endif

    void mamba_create(const char* const* specs, const char* prefix = "");

    void mamba_install(const char* const* specs, const char* prefix = "");

    void mamba_update(const char* const* specs, const char* prefix = "");

    void mamba_list(const char* regex = "", const char* prefix = "");

    void mamba_info(const char* prefix = "");

    void mamba_shell(const char* action, const char* shell_type, const char* prefix = "");

    void mamba_config_list();

    void mamba_set_config(const char* name, const char* value);

    void mamba_clear_config(const char* name);

    void mamba_set_opt(int option, int value);

    int const MAMBA_USE_INDEX_CACHE = 1;
    int const MAMBA_OFFLINE = 2;
    int const MAMBA_QUIET = 3;
    int const MAMBA_JSON = 4;
    int const MAMBA_AUTO_ACTIVATE_BASE = 5;
    int const MAMBA_DEV = 6;
    int const MAMBA_ON_CI = 7;
    int const MAMBA_NO_PROGRESS_BARS = 8;
    int const MAMBA_DRY_RUN = 9;
    int const MAMBA_ALWAYS_YES = 10;
    int const MAMBA_KEEP_TEMP_FILES = 11;
    int const MAMBA_KEEP_TEMP_DIRECTORIES = 12;
    int const MAMBA_CHANGE_PS1 = 13;
    int const MAMBA_ADD_PIP_AS_PYTHON_DEPENDENCY = 14;
    int const MAMBA_VERBOSITY = 14;

    // std::size_t MAMBA_LOCAL_REPODATA_TTL = 1; // take from header
    // long MAMBA_MAX_PARALLEL_DOWNLOADS = 5;
    // bool const MAMBA_SIG_INTERRUPT = false;
    // int MAMBA_CONNECT_TIMEOUT_SECS = 10;
    // int MAMBA_RETRY_TIMEOUT = 2; // seconds
    // int MAMBA_RETRY_BACKOFF = 3; // retry_timeout * retry_backoff
    // int MAMBA_MAX_RETRIES = 3; // max number of retries

#ifdef __cplusplus
}
#endif

#endif
