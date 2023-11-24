// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/download/downloader.hpp"

#include "mambatests.hpp"

namespace mamba
{
    TEST_SUITE("downloader")
    {
        TEST_CASE("file_does_not_exist")
        {
#ifdef __linux__
            download::Request request(
                "test",
                download::MirrorName(""),
                "file:///nonexistent/repodata.json",
                "test_download_repodata.json",
                false,
                true
            );
            auto& context = mambatests::singletons().context;
            const auto previous_quiet = context.output_params.quiet;
            auto _ = on_scope_exit([&] { context.output_params.quiet = previous_quiet; });

            download::MultiRequest dl_request{ std::vector{ std::move(request) } };
            context.output_params.quiet = true;
            download::MultiResult res = download::download(dl_request, context.mirrors, context);
            CHECK_EQ(res.size(), std::size_t(1));
            CHECK(!res[0]);
            CHECK_EQ(res[0].error().attempt_number, std::size_t(1));
#endif
        }

        TEST_CASE("file_does_not_exist_throw")
        {
#ifdef __linux__
            download::Request request(
                "test",
                download::MirrorName(""),
                "file:///nonexistent/repodata.json",
                "test_download_repodata.json"
            );
            download::MultiRequest dl_request{ std::vector{ std::move(request) } };
            auto& context = mambatests::singletons().context;
            const auto previous_quiet = context.output_params.quiet;
            auto _ = on_scope_exit([&] { context.output_params.quiet = previous_quiet; });
            context.output_params.quiet = true;
            CHECK_THROWS_AS(download::download(dl_request, context.mirrors, context), std::runtime_error);
#endif
        }
    }
}
