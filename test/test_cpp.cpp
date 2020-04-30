#include <gtest/gtest.h>

#include "match_spec.hpp"
#include "context.hpp"
#include "link.hpp"

namespace mamba
{
    // TEST(cpp_install, install)
    // {
    //     Context::instance().set_verbosity(3);
    //     PackageInfo pkg("wheel", "0.34.2", "py_1", 1);
    //     fs::path prefix = "C:\\Users\\wolfv\\miniconda3\\";
    //     TransactionContext tc(prefix, "3.8.x");
    //     // try {
    //         UnlinkPackage up(pkg, &tc);
    //         up.execute();
    //     // } catch (...) { std::cout << "Nothing to delete ... \n"; }
    //     LinkPackage lp(pkg, prefix / "pkgs" , &tc);
    //     lp.execute();
    // }

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