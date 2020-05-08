#include <gtest/gtest.h>

#include "util.hpp"
#include "subdirdata.hpp"

namespace mamba
{
    TEST(transfer, file_not_exist)
    {
        #ifdef __linux__
        Context::instance().quiet = true;
        {
            mamba::MultiDownloadTarget multi_dl;
            mamba::MSubdirData cf("conda-forge/linux-64",
                                  "file:///nonexistent/repodata.json",
                                  "/tmp/zyx.json");
            cf.load();
            multi_dl.add(cf.target());
            multi_dl.download(true);
        }
        {
            mamba::MultiDownloadTarget multi_dl;
            mamba::MSubdirData cf("conda-forge/noarch",
                                  "file:///nonexistent/repodata.json",
                                  "/tmp/zyx.json");
            cf.load();
            multi_dl.add(cf.target());
            EXPECT_THROW(multi_dl.download(true), std::runtime_error);
        }
        Context::instance().quiet = false;
        #endif
    }
}