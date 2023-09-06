// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>

#include <doctest/doctest.h>

#include "mamba/core/subdirdata.hpp"

#include "mambatests.hpp"

namespace mamba
{
    TEST_SUITE("transfer")
    {
        TEST_CASE("file_not_exist")
        {
#ifdef __linux__
            auto& context = mambatests::context();
            context.output_params.quiet = true;
            {
                mamba::ChannelContext channel_context{ context };
                const mamba::Channel& c = channel_context.make_channel("conda-forge");
                mamba::MultiDownloadTarget multi_dl{ context };
                mamba::MultiPackageCache pkg_cache({ "/tmp/" }, context.validation_params);
                mamba::MSubdirData cf = mamba::MSubdirData::create(
                                            channel_context,
                                            c,
                                            "linux-64",
                                            "file:///nonexistent/repodata.json",
                                            pkg_cache
                )
                                            .value();
                multi_dl.add(cf.target());

                // file:// url should not retry
                CHECK_EQ(cf.target()->can_retry(), false);

                multi_dl.download(MAMBA_DOWNLOAD_FAILFAST);

                // File does not exist
                CHECK_EQ(cf.target()->get_result(), 37);
            }
            {
                mamba::ChannelContext channel_context{ context };
                const mamba::Channel& c = channel_context.make_channel("conda-forge");
                mamba::MultiDownloadTarget multi_dl{ channel_context.context() };
                mamba::MultiPackageCache pkg_cache({ "/tmp/" }, context.validation_params);
                mamba::MSubdirData cf = mamba::MSubdirData::create(
                                            channel_context,
                                            c,
                                            "noarch",
                                            "file:///nonexistent/repodata.json",
                                            pkg_cache
                )
                                            .value();
                multi_dl.add(cf.target());
                CHECK_THROWS_AS(multi_dl.download(MAMBA_DOWNLOAD_FAILFAST), std::runtime_error);
            }
            context.output_params.quiet = false;
#endif
        }
    }
}  // namespace mamba
