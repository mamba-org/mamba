
#include <gtest/gtest.h>

#include <sstream>
#include <tuple>

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"

namespace mamba
{
    TEST(output, no_progress_bars)
    {
        Context::instance().no_progress_bars = true;
        auto proxy = Console::instance().add_progress_bar("conda-forge");
        EXPECT_FALSE(proxy.defined());
        EXPECT_FALSE(proxy);

        Context::instance().no_progress_bars = false;
        proxy = Console::instance().add_progress_bar("conda-forge");
        EXPECT_TRUE(proxy.defined());
        EXPECT_TRUE(proxy);
    }
}  // namespace mamba
