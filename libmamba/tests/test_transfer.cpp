#include <gtest/gtest.h>

#include "mamba/core/subdirdata.hpp"

namespace mamba
{
    //     TEST(transfer, file_not_exist)
    //     {
    // #ifdef __linux__
    //         Context::instance().quiet = true;
    //         {
    //             const mamba::Channel& c = mamba::make_channel("conda-forge");
    //             mamba::MultiDownloadTarget multi_dl;
    //             mamba::MultiPackageCache pkg_cache({ "/tmp/" });
    //             mamba::MSubdirData cf
    //                 = mamba::MSubdirData::create(
    //                       c, "linux-64", "file:///nonexistent/repodata.json", pkg_cache)
    //                       .value();
    //             multi_dl.add(cf.target());

    //             // file:// url should not retry
    //             EXPECT_EQ(cf.target()->can_retry(), false);

    //             multi_dl.download(MAMBA_DOWNLOAD_FAILFAST);

    //             // File does not exist
    //             EXPECT_EQ(cf.target()->result, 37);
    //         }
    //         {
    //             const mamba::Channel& c = mamba::make_channel("conda-forge");
    //             mamba::MultiDownloadTarget multi_dl;
    //             mamba::MultiPackageCache pkg_cache({ "/tmp/" });
    //             mamba::MSubdirData cf = mamba::MSubdirData::create(
    //                                         c, "noarch", "file:///nonexistent/repodata.json",
    //                                         pkg_cache) .value();
    //             multi_dl.add(cf.target());
    //             EXPECT_THROW(multi_dl.download(MAMBA_DOWNLOAD_FAILFAST), std::runtime_error);
    //         }
    //         Context::instance().quiet = false;
    // #endif
    //     }
}  // namespace mamba
