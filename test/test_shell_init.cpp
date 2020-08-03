#include <gtest/gtest.h>

#include "shell_init.hpp"
#include "util.hpp"

namespace mamba
{
    TEST(shell_init, init)
    {
        // std::cout << rcfile_content("/home/wolfv/miniconda3/", "bash");
    }

    TEST(shell_init, bashrc_modifications)
    {
        // modify_rc_file("/home/wolfv/Programs/mamba/test/.bashrc",
        // "/home/wolfv/superconda/", "bash");
    }

    TEST(shell_init, expand_user)
    {
        auto expanded = env::expand_user("~/this/is/a/test");
        if (on_linux)
        {
            EXPECT_TRUE(starts_with(expanded.string(), "/home/"));
        }
    }
}  // namespace mamba
