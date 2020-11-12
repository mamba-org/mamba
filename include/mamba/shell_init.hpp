// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SHELL_INIT
#define MAMBA_SHELL_INIT

#include <string>

#include "mamba_fs.hpp"

namespace mamba
{
    std::string guess_shell();

#ifdef _WIN32
    void init_cmd_exe_registry(const std::wstring& reg_path,
                               const fs::path& conda_prefix,
                               bool reverse = false);
#endif

    fs::path get_self_exe_path();
    std::string get_hook_contents(const std::string& shell);

    // this function calls cygpath to convert win path to unix
    std::string native_path_to_unix(const std::string& path, bool is_a_path_env = false);

    std::string rcfile_content(const fs::path& env_prefix,
                               const std::string& shell,
                               const fs::path& mamba_exe);

    std::string xonsh_content(const fs::path& env_prefix,
                              const std::string& shell,
                              const fs::path& mamba_exe);

    bool modify_rc_file(const fs::path& file_path,
                        const fs::path& conda_prefix,
                        const std::string& shell,
                        const fs::path& mamba_exe);

    // we need this function during linking...
    void init_root_prefix_cmdexe(const fs::path& root_prefix);
    void init_root_prefix(const std::string& shell, const fs::path& root_prefix);

    std::string powershell_contents(const fs::path& conda_prefix);
    bool init_powershell(const fs::path& profile_path,
                         const fs::path& conda_prefix,
                         bool reverse = false);

    void init_shell(const std::string& shell, const fs::path& conda_prefix);
}

#endif
