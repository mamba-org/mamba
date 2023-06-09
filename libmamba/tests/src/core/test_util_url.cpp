// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#include <doctest/doctest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/util_url.hpp"

namespace mamba
{
    TEST_SUITE("util_url")
    {
        TEST_CASE("concat_scheme_url")
        {
            auto url = concat_scheme_url("https", "mamba.com");
            CHECK_EQ(url, "https://mamba.com");

            url = concat_scheme_url("file", "C:/some_folder");
            CHECK_EQ(url, "file:///C:/some_folder");

            url = concat_scheme_url("file", "some_folder");
            CHECK_EQ(url, "file://some_folder");
        }

        TEST_CASE("build_url")
        {
            auto url = build_url(std::nullopt, "https", "mamba.com", true);
            CHECK_EQ(url, "https://mamba.com");

            url = build_url(std::nullopt, "https", "mamba.com", false);
            CHECK_EQ(url, "https://mamba.com");

            url = build_url(std::optional<std::string>("auth"), "https", "mamba.com", false);
            CHECK_EQ(url, "https://mamba.com");

            url = build_url(std::optional<std::string>("auth"), "https", "mamba.com", true);
            CHECK_EQ(url, "https://auth@mamba.com");

            url = build_url(std::optional<std::string>(""), "https", "mamba.com", true);
            CHECK_EQ(url, "https://@mamba.com");
        }

        TEST_CASE("split_platform")
        {
            auto& ctx = Context::instance();

            std::string platform_found, cleaned_url;
            split_platform(
                { "noarch", "linux-64" },
                "https://mamba.com/linux-64/package.tar.bz2",
                ctx.platform,
                cleaned_url,
                platform_found
            );

            CHECK_EQ(platform_found, "linux-64");
            CHECK_EQ(cleaned_url, "https://mamba.com/package.tar.bz2");

            split_platform(
                { "noarch", "linux-64" },
                "https://mamba.com/linux-64/noarch-package.tar.bz2",
                ctx.platform,
                cleaned_url,
                platform_found
            );
            CHECK_EQ(platform_found, "linux-64");
            CHECK_EQ(cleaned_url, "https://mamba.com/noarch-package.tar.bz2");

            split_platform(
                { "linux-64", "osx-arm64", "noarch" },
                "https://mamba.com/noarch/kernel_linux-64-package.tar.bz2",
                ctx.platform,
                cleaned_url,
                platform_found
            );
            CHECK_EQ(platform_found, "noarch");
            CHECK_EQ(cleaned_url, "https://mamba.com/kernel_linux-64-package.tar.bz2");

            split_platform(
                { "noarch", "linux-64" },
                "https://mamba.com/linux-64",
                ctx.platform,
                cleaned_url,
                platform_found
            );

            CHECK_EQ(platform_found, "linux-64");
            CHECK_EQ(cleaned_url, "https://mamba.com");

            split_platform(
                { "noarch", "linux-64" },
                "https://mamba.com/noarch",
                ctx.platform,
                cleaned_url,
                platform_found
            );

            CHECK_EQ(platform_found, "noarch");
            CHECK_EQ(cleaned_url, "https://mamba.com");
        }
    }
}  // namespace mamba
