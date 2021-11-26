// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_UTIL_OS_HPP
#define MAMBA_CORE_UTIL_OS_HPP

#include "mamba/core/fsutil.hpp"
#include <string>

namespace mamba
{
    bool is_admin();
    fs::path get_self_exe_path();

#ifndef _WIN32
    std::string get_process_name_by_pid(const int pid);
#else
    DWORD getppid();
    std::string get_process_name_by_pid(DWORD processId);
#endif

    void run_as_admin(const std::string& args);
    bool enable_long_paths_support(bool force);
    std::string windows_version();
    std::string macos_version();
    std::string linux_version();

    void init_console();
    void reset_console();

#ifdef _WIN32
    std::string to_utf8(const wchar_t* w, size_t s);
    std::string to_utf8(const wchar_t* w);
#endif
}

#endif
