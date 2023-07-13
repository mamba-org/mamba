#include <doctest/doctest.h>

#include "mamba/core/activation.hpp"
#include "mamba/core/context.hpp"

namespace mamba
{
    TEST_SUITE("activation")
    {
        TEST_CASE("activation")
        {
            PosixActivator activator{ Context::instance() };
            // std::cout << a.add_prefix_to_path("/home/wolfv/miniconda3", 0) <<
            // std::endl; std::cout << a.activate("/home/wolfv/miniconda3/", false) <<
            // std::endl;
        }
    }
}  // namespace mamba
