// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <sys/wait.h>
#include <unistd.h>
#if __linux__
#include <sys/prctl.h>
#endif

#include <catch2/catch_all.hpp>

#include "mamba/core/shell_init.hpp"
#include "mamba/util/environment.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("guess_shell with SHELL env var")
        {
            const auto restore = mambatests::EnvironmentCleaner();

            SECTION("bash")
            {
                util::set_env("SHELL", "/bin/bash");
                REQUIRE(guess_shell() == "bash");
            }

            SECTION("zsh")
            {
                util::set_env("SHELL", "/usr/bin/zsh");
                REQUIRE(guess_shell() == "zsh");
            }

            SECTION("csh")
            {
                util::set_env("SHELL", "/bin/csh");
                REQUIRE(guess_shell() == "csh");
            }

            SECTION("dash")
            {
                util::set_env("SHELL", "/bin/dash");
                REQUIRE(guess_shell() == "dash");
            }

            SECTION("nu")
            {
                util::set_env("SHELL", "/usr/bin/nu");
                REQUIRE(guess_shell() == "nu");
            }

            SECTION("nushell")
            {
                util::set_env("SHELL", "/usr/bin/nushell");
                REQUIRE(guess_shell() == "nushell");
            }

            SECTION("xonsh")
            {
                util::set_env("SHELL", "/usr/bin/xonsh");
                REQUIRE(guess_shell() == "xonsh");
            }

            SECTION("cmd.exe")
            {
                util::set_env("SHELL", "/usr/bin/cmd.exe");
                REQUIRE(guess_shell() == "cmd.exe");
            }

            SECTION("pwsh")
            {
                util::set_env("SHELL", "/usr/bin/pwsh");
                REQUIRE(guess_shell() == "pwsh");
            }

            SECTION("fish")
            {
                util::set_env("SHELL", "/usr/bin/fish");
                REQUIRE(guess_shell() == "fish");
            }

            SECTION("sh")
            {
                util::set_env("SHELL", "/bin/sh");
                REQUIRE(guess_shell() == "sh");
            }

            SECTION("bash without path prefix")
            {
                util::set_env("SHELL", "bash");
                REQUIRE(guess_shell() == "bash");
            }

            SECTION("nu without path prefix")
            {
                util::set_env("SHELL", "nu");
                REQUIRE(guess_shell() == "nu");
            }
        }

#if __linux__
        void assert_guess_shell_with_parent_name(const char* parent_name, const std::string& expected)
        {
            // Set the process name
            prctl(PR_SET_NAME, parent_name, 0, 0, 0);

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
                assert_guess_shell_with_parent_name("Miniforge3-Linux-x86_64", "");
            }

            SECTION("Parent named bash")
            {
                assert_guess_shell_with_parent_name("bash", "bash");
            }

            SECTION("Parent named nu")
            {
                assert_guess_shell_with_parent_name("nu", "nu");
            }

            SECTION("Parent named nushell")
            {
                assert_guess_shell_with_parent_name("nushell", "nu");
            }

            SECTION("Parent named my-nushell-process")
            {
                assert_guess_shell_with_parent_name("my-nushell-process", "nu");
            }

            SECTION("Parent named nu_server")
            {
                assert_guess_shell_with_parent_name("nu_server", "");
            }
        }
#endif
    }
}  // namespace mamba
