// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/api/configuration.hpp"
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
            REQUIRE(res.size() == std::size_t(1));
            REQUIRE(!res[0]);
            REQUIRE(res[0].error().attempt_number == std::size_t(1));
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

        TEST_CASE("Use CA certificate from the root prefix")
        {
            // Create a context, make a request and check that ssl_verify is set to the correct path
            auto& context = mambatests::singletons().context;

            // Set the context values to the default ones
            context.remote_fetch_params.curl_initialized = false;
            context.remote_fetch_params.ssl_verify = "<system>";

            download::Request request(
                "test",
                download::MirrorName(""),
                "https://conda.anaconda.org/conda-forge/linux-64/repodata.json",
                "test_download_repodata.json"
            );
            download::MultiRequest dl_request{ std::vector{ std::move(request) } };

            // Downloading must initialize curl and set `ssl_verify` to the path of the CA
            // certificate
            REQUIRE(!context.remote_fetch_params.curl_initialized);
            download::MultiResult res = download::download(dl_request, context.mirrors, context);
            REQUIRE(context.remote_fetch_params.curl_initialized);

            auto certificates = context.remote_fetch_params.ssl_verify;
            const fs::u8path root_prefix = detail::get_root_prefix();
            const fs::u8path expected_certificates = root_prefix / "ssl" / "cert.pem";

            // TODO: is libmamba tested without a root prefix or a base installation?
            bool reach_fallback_certificates;
            if (util::on_win)
            {
                // Default certificates from libcurl/libssl are used on Windows
                reach_fallback_certificates = certificates == "";
            }
            else
            {
                reach_fallback_certificates = (mamba::util::ends_with(certificates, "cert.pem") || mamba::util::ends_with(certificates, "ca-certificates.crt"));
            }
            REQUIRE((certificates == expected_certificates || reach_fallback_certificates));
        }
    }
}
