// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/download/downloader.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("file_does_not_exist")
        {
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
            REQUIRE(res.size() == std::size_t(1);
            REQUIRE(!res[0]);
            REQUIRE(res[0].error().attempt_number == std::size_t(1);
        }

        TEST_CASE("file_does_not_exist_throw")
        {
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
            REQUIRE_THROWS_AS(
                download::download(dl_request, context.mirrors, context),
                std::runtime_error
            );
        }
    }
}
