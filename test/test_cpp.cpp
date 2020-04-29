#include <gtest/gtest.h>

#include "match_spec.hpp"

namespace mamba
{
    TEST(match_spec, parse)
    {
        {
            MatchSpec ms("xtensor==0.12.3");   
            EXPECT_EQ(ms.version, "==0.12.3");
            EXPECT_EQ(ms.name, "xtensor");
        }
        {
            MatchSpec ms("numpy 1.7*");   
            EXPECT_EQ(ms.version, "1.7*");
            EXPECT_EQ(ms.name, "numpy");
        }
        {
            MatchSpec ms("numpy[version='1.7|1.8']");   
            // TODO!
            // EXPECT_EQ(ms.version, "1.7|1.8");
            EXPECT_EQ(ms.name, "numpy");
            EXPECT_EQ(ms.brackets["version"], "1.7|1.8");
        }
        {
            MatchSpec ms("conda-forge/linux64::xtensor==0.12.3");   
            EXPECT_EQ(ms.version, "==0.12.3");
            EXPECT_EQ(ms.name, "xtensor");
            // EXPECT_EQ(ms.channel, "conda-forge/linux64");
        }
    }
}