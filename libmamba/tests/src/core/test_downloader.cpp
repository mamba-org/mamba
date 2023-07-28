// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/core/download.hpp"

namespace mamba
{
    TEST_SUITE("downloader")
    {
        TEST_CASE("file_does_not_exist")
        {
#ifdef __linux__
            DownloadRequest request(
                "test",
                "file:///nonexistent/repodata.json",
                "test_download_repodata.json",
                false,
                true
            );
            MultiDownloadRequest dl_request{ std::vector{ std::move(request) } };
            Context::instance().output_params.quiet = true;
            MultiDownloadResult res = download(dl_request, Context::instance());
            CHECK_EQ(res.results.size(), std::size_t(1));
            CHECK(!res.results[0]);
            CHECK_EQ(res.results[0].error().attempt_number, std::size_t(1));
#endif
        }

        TEST_CASE("file_does_not_exist_throw")
        {
#ifdef __linux__
            DownloadRequest request(
                "test",
                "file:///nonexistent/repodata.json",
                "test_download_repodata.json"
            );
            MultiDownloadRequest dl_request{ std::vector{ std::move(request) } };
            Context::instance().output_params.quiet = true;
            CHECK_THROWS_AS(download(dl_request, Context::instance()), std::runtime_error);
#endif
        }
    }
}
