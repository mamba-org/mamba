#include <gtest/gtest.h>

#include "mamba/core/subdirdata.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{
    TEST(transfer, file_not_exist)
    {
#ifdef __linux__
        Context::instance().quiet = true;
        {
            mamba::MultiDownloadTarget multi_dl;
            mamba::MultiPackageCache pkg_cache({ "/tmp/" });
            mamba::MSubdirData cf("conda-forge/linux-64",
                                  "file:///nonexistent/repodata.json",
                                  "zyx.json",
                                  pkg_cache,
                                  false);
            cf.load();
            multi_dl.add(cf.target());

            // file:// url should not retry
            EXPECT_EQ(cf.target()->can_retry(), false);

            multi_dl.download(MAMBA_DOWNLOAD_FAILFAST);

            // File does not exist
            EXPECT_EQ(cf.target()->result, 37);
        }
        {
            mamba::MultiDownloadTarget multi_dl;
            mamba::MultiPackageCache pkg_cache({ "/tmp/" });
            mamba::MSubdirData cf("conda-forge/noarch",
                                  "file:///nonexistent/repodata.json",
                                  "zyx.json",
                                  pkg_cache,
                                  true);
            cf.load();
            multi_dl.add(cf.target());
            EXPECT_THROW(multi_dl.download(MAMBA_DOWNLOAD_FAILFAST), std::runtime_error);
        }
        Context::instance().quiet = false;
#endif
    }
}  // namespace mamba
