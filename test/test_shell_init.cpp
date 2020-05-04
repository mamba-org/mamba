#include <gtest/gtest.h>

#include "shell_init.hpp"

namespace mamba
{
    TEST(shell_init, init)
    {
        // std::cout << rcfile_content("/home/wolfv/miniconda3/", "bash");
    }

    TEST(shell_init, bashrc_modifications)
    {
        // modify_rc_file("/home/wolfv/Programs/mamba/test/.bashrc", "/home/wolfv/superconda/", "bash");
    }

    TEST(shell_init, expand_path)
    {
        auto expanded = expand_path("~/this/is/a/test");
        std::cout << expanded << std::endl;
    }
}