// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/api/configuration.hpp"
#include "mamba/core/util.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("file_does_not_exist", "[mamba::download]")
        {
            download::Request request(
                "test",
                download::MirrorName(""),
                "file:///nonexistent/repodata.json",
                "test_download_repodata_1.json",
                false,
                true
            );

            download::MultiRequest dl_request{ std::vector{ std::move(request) } };
            download::MultiResult res = download::download(dl_request, {}, {}, {});
            REQUIRE(res.size() == std::size_t(1));
            REQUIRE(!res[0]);
            REQUIRE(res[0].error().attempt_number == std::size_t(1));
        }

        TEST_CASE("file_does_not_exist_throw", "[mamba::download]")
        {
            download::Request request(
                "test",
                download::MirrorName(""),
                "file:///nonexistent/repodata.json",
                "test_download_repodata_2.json"
            );
            download::MultiRequest dl_request{ std::vector{ std::move(request) } };
            REQUIRE_THROWS_AS(download::download(dl_request, {}, {}, {}), std::runtime_error);
        }

        TEST_CASE("Use CA certificate from the root prefix", "[mamba::download]")
        {
            const auto tmp_dir = TemporaryDirectory();

            // Set the context values to the default ones
            auto params = download::RemoteFetchParams{};
            params.curl_initialized = false;
            params.ssl_verify = "<system>";

            download::Request request(
                "test",
                download::MirrorName(""),
                "https://conda.anaconda.org/conda-forge/linux-64/repodata.json",
                tmp_dir.path() / "test_download_repodata_3.json"
            );
            download::MultiRequest dl_request{ std::vector{ std::move(request) } };

            // Downloading must initialize curl and set `ssl_verify` to the path of the CA
            // certificate
            REQUIRE(!params.curl_initialized);
            download::MultiResult res = download::download(dl_request, {}, params, {});
            REQUIRE(params.curl_initialized);

            auto certificates = params.ssl_verify;
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
