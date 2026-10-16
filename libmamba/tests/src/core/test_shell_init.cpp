// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#if __linux__
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <catch2/catch_all.hpp>

#include "mamba/core/shell_init.hpp"
#include "mamba/util/environment.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
#if __linux__
        void
        assert_guess_shell_with_parent_process_name(const char* pp_name, const std::string& expected)
        {
            // Set the process name
            prctl(PR_SET_NAME, pp_name, 0, 0, 0);

            // Create a child process
            pid_t pid = fork();
            REQUIRE(pid >= 0);  // Success

            // Child process case
            if (pid == 0)
            {
                std::string shell = guess_shell();
                _exit(shell == expected ? 0 : 1);
            }

            int status;
            waitpid(pid, &status, 0);
            REQUIRE(WIFEXITED(status));
            REQUIRE(WEXITSTATUS(status) == 0);
        }

        TEST_CASE("guess_shell from parent process name when SHELL is unset")
        {
            const auto restore = mambatests::EnvironmentCleaner();
            util::unset_env("SHELL");

            SECTION("Parent process named Miniforge3-Linux-x86_64")
            {
                // Reproduce https://github.com/mamba-org/mamba/issues/4375
                // Set the process name to a string containing "nu" (not as a whole word)
                // Guessed shell should be empty
                assert_guess_shell_with_parent_process_name("Miniforge3-Linux-x86_64", "");
            }

            SECTION("Parent named bash")
            {
                assert_guess_shell_with_parent_process_name("bash", "bash");
            }

            SECTION("Parent named nu")
            {
                assert_guess_shell_with_parent_process_name("nu", "nu");
            }

            SECTION("Parent named nushell")
            {
                assert_guess_shell_with_parent_process_name("nushell", "nu");
            }

            SECTION("Parent named my-nushell-process")
            {
                assert_guess_shell_with_parent_process_name("my-nushell-process", "nu");
            }

            SECTION("Parent named nu_server")
            {
                assert_guess_shell_with_parent_process_name("nu_server", "");
            }
        }

        TEST_CASE("guess_shell with SHELL env var as fallback")
        {
            const auto restore = mambatests::EnvironmentCleaner();
            util::unset_env("SHELL");

            SECTION("bash")
            {
                util::set_env("SHELL", "/bin/bash");
                assert_guess_shell_with_parent_process_name("mamba-test", "bash");
            }

            SECTION("zsh")
            {
                util::set_env("SHELL", "/usr/bin/zsh");
                assert_guess_shell_with_parent_process_name("mamba-test", "zsh");
            }

            SECTION("csh")
            {
                util::set_env("SHELL", "/bin/csh");
                assert_guess_shell_with_parent_process_name("mamba-test", "csh");
            }

            SECTION("dash")
            {
                util::set_env("SHELL", "/bin/dash");
                assert_guess_shell_with_parent_process_name("mamba-test", "dash");
            }

            SECTION("nu")
            {
                util::set_env("SHELL", "/usr/bin/nu");
                assert_guess_shell_with_parent_process_name("mamba-test", "nu");
            }

            SECTION("nushell")
            {
                util::set_env("SHELL", "/usr/bin/nushell");
                assert_guess_shell_with_parent_process_name("mamba-test", "nushell");
            }

            SECTION("xonsh")
            {
                util::set_env("SHELL", "/usr/bin/xonsh");
                assert_guess_shell_with_parent_process_name("mamba-test", "xonsh");
            }

            SECTION("cmd.exe")
            {
                util::set_env("SHELL", "/usr/bin/cmd.exe");
                assert_guess_shell_with_parent_process_name("mamba-test", "cmd.exe");
            }

            SECTION("pwsh")
            {
                util::set_env("SHELL", "/usr/bin/pwsh");
                assert_guess_shell_with_parent_process_name("mamba-test", "pwsh");
            }

            SECTION("fish")
            {
                util::set_env("SHELL", "/usr/bin/fish");
                assert_guess_shell_with_parent_process_name("mamba-test", "fish");
            }

            SECTION("sh")
            {
                util::set_env("SHELL", "/bin/sh");
                assert_guess_shell_with_parent_process_name("mamba-test", "sh");
            }

            SECTION("bash without path prefix")
            {
                util::set_env("SHELL", "bash");
                assert_guess_shell_with_parent_process_name("mamba-test", "bash");
            }

            SECTION("nu without path prefix")
            {
                util::set_env("SHELL", "nu");
                assert_guess_shell_with_parent_process_name("mamba-test", "nu");
            }
        }
#endif
    }
}  // namespace mamba
