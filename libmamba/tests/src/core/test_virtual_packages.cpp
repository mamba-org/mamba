// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <ranges>
#include <string_view>

#include <catch2/catch_all.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace testing
    {

        namespace
        {
            auto
            has_virtual_package(const std::vector<specs::PackageInfo>& pkgs, std::string_view name)
                -> bool
            {
                return std::ranges::any_of(pkgs, [&](const auto& pkg) { return pkg.name == name; });
            }

            auto
            require_virtual_package(const std::vector<specs::PackageInfo>& pkgs, std::string_view name)
                -> const specs::PackageInfo&
            {
                const auto it = std::ranges::find_if(
                    pkgs,
                    [&](const auto& pkg) { return pkg.name == name; }
                );
                REQUIRE(it != pkgs.end());
                return *it;
            }

            TEST_CASE("make_virtual_package")
            {
                const auto& context = mambatests::context();
                const auto pkg = detail::make_virtual_package("test", context.platform, "0.1.5", "abcd");

                REQUIRE(pkg.name == "test");
                REQUIRE(pkg.version == "0.1.5");
                REQUIRE(pkg.build_string == "abcd");
                REQUIRE(pkg.build_number == 0);
                REQUIRE(pkg.channel == "@");
                REQUIRE(pkg.platform == context.platform);
                REQUIRE(pkg.md5 == "12345678901234567890123456789012");
                REQUIRE(pkg.filename == pkg.name);
            }

            TEST_CASE("dist_packages")
            {
                using Version = specs::Version;
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});

                auto& ctx = mambatests::context();
                mambatests::ScopedContextChange context_change{ ctx };
                context_change.preserve(ctx.platform);

                const override_virtual_packages_map no_overrides{};
                auto pkgs = detail::dist_packages(ctx.platform, no_overrides);

                if (util::on_win)
                {
                    REQUIRE(pkgs.size() == 2);
                    REQUIRE(pkgs[0].name == "__win");
                    REQUIRE(Version::parse(pkgs[0].version).value() > Version());
                }
                if (util::on_linux)
                {
                    REQUIRE(pkgs.size() == 4);
                    REQUIRE(pkgs[0].name == "__unix");
                    REQUIRE(pkgs[1].name == "__linux");
                    REQUIRE(Version::parse(pkgs[1].version).value() > Version());
                    REQUIRE(pkgs[2].name == "__glibc");
                    REQUIRE(Version::parse(pkgs[2].version).value() > Version());
                }
                if (util::on_mac)
                {
                    REQUIRE(pkgs.size() == 3);
                    REQUIRE(pkgs[0].name == "__unix");
                    REQUIRE(pkgs[1].name == "__osx");
                    CHECK(Version::parse(pkgs[1].version).value() > Version());
                }
#if __x86_64__ || defined(_WIN64)
                REQUIRE(pkgs.back().name == "__archspec");
                REQUIRE(pkgs.back().build_string.find("x86_64") == 0);
#endif

                util::set_env("CONDA_OVERRIDE_OSX", "12.1");
                pkgs = detail::dist_packages("osx-arm", no_overrides);
                REQUIRE(pkgs.size() == 3);
                REQUIRE(pkgs[0].name == "__unix");
                REQUIRE(pkgs[1].name == "__osx");
                REQUIRE(pkgs[1].version == "12.1");
                REQUIRE(pkgs[2].name == "__archspec");
                REQUIRE(pkgs[2].build_string == "arm");

                util::unset_env("CONDA_OVERRIDE_OSX");
                util::set_env("CONDA_OVERRIDE_LINUX", "5.7");
                util::set_env("CONDA_OVERRIDE_GLIBC", "2.15");
                pkgs = detail::dist_packages("linux-32", no_overrides);
                REQUIRE(pkgs.size() == 4);
                REQUIRE(pkgs[0].name == "__unix");
                REQUIRE(pkgs[1].name == "__linux");
                REQUIRE(pkgs[1].version == "5.7");
                REQUIRE(pkgs[2].name == "__glibc");
                REQUIRE(pkgs[2].version == "2.15");
                REQUIRE(pkgs[3].name == "__archspec");
                REQUIRE(pkgs[3].build_string == "x86");
                util::unset_env("CONDA_OVERRIDE_GLIBC");
                util::unset_env("CONDA_OVERRIDE_LINUX");

                pkgs = detail::dist_packages("lin-850", no_overrides);
                REQUIRE(pkgs.size() == 1);
                REQUIRE(pkgs[0].name == "__archspec");
                REQUIRE(pkgs[0].build_string == "850");
                util::unset_env("CONDA_SUBDIR");

                pkgs = detail::dist_packages("linux", no_overrides);
                REQUIRE(pkgs.size() == 0);

                // Test emscripten platform
                pkgs = detail::dist_packages("emscripten-wasm32", no_overrides);
                REQUIRE(pkgs.size() == 2);
                REQUIRE(pkgs[0].name == "__unix");
                REQUIRE(pkgs[1].name == "__archspec");
                REQUIRE(pkgs[1].build_string == "wasm32");
            }

            TEST_CASE("get_virtual_packages")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});

                // Use a fixed platform to isolate from shared context state.
                // Use "linux-64" with explicit overrides so dist_packages returns
                // a deterministic set regardless of host OS.
                const std::string platform = "linux-64";
                const override_virtual_packages_map overrides = {
                    { "linux", "5.15" },
                    { "glibc", "2.35" },
                    { "archspec", "x86_64_v3" },
                };

                // Suppress any host CUDA detection so the baseline is predictable.
                util::set_env("CONDA_OVERRIDE_CUDA", "");
                auto baseline = get_virtual_packages(platform, overrides);
                const size_t baseline_count = baseline.size();
                REQUIRE(baseline_count == 4);

                // With an explicit CUDA env override, __cuda is appended.
                util::set_env("CONDA_OVERRIDE_CUDA", "9.0");
                auto pkgs = get_virtual_packages(platform, overrides);
                REQUIRE(pkgs.size() == baseline_count + 1);
                REQUIRE(pkgs.back().name == "__cuda");
                REQUIRE(pkgs.back().version == "9.0");

                // Suppressing CUDA again removes __cuda.
                util::set_env("CONDA_OVERRIDE_CUDA", "");
                pkgs = get_virtual_packages(platform, overrides);
                REQUIRE(pkgs.size() == baseline_count);
            }

            TEST_CASE("override_virtual_packages_from_config")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});

                const override_virtual_packages_map overrides = {
                    { "cuda", "13.1" }, { "glibc", "2.15" },         { "linux", "5.7" },
                    { "osx", "12.1" },  { "archspec", "x86_64_v4" },
                };

                auto pkgs = get_virtual_packages("linux-64", overrides);
                REQUIRE(pkgs.size() == 5);
                REQUIRE(pkgs[0].name == "__unix");
                REQUIRE(pkgs[1].name == "__linux");
                REQUIRE(pkgs[1].version == "5.7");
                REQUIRE(pkgs[2].name == "__glibc");
                REQUIRE(pkgs[2].version == "2.15");
                REQUIRE(pkgs[3].name == "__archspec");
                REQUIRE(pkgs[3].version == "1");
                REQUIRE(pkgs[3].build_string == "x86_64_v4");
                REQUIRE(pkgs[4].name == "__cuda");
                REQUIRE(pkgs[4].version == "13.1");

                pkgs = get_virtual_packages("osx-64", overrides);
                REQUIRE(pkgs.size() == 4);
                REQUIRE(pkgs[0].name == "__unix");
                REQUIRE(pkgs[1].name == "__osx");
                REQUIRE(pkgs[1].version == "12.1");
                REQUIRE(pkgs[2].name == "__archspec");
                REQUIRE(pkgs[2].version == "1");
                REQUIRE(pkgs[2].build_string == "x86_64_v4");
                REQUIRE(pkgs[3].name == "__cuda");
                REQUIRE(pkgs[3].version == "13.1");
            }

            TEST_CASE("override_virtual_packages_dunder_prefix")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});

                // Keys with `__` prefix are accepted and applied.
                const override_virtual_packages_map overrides = {
                    { "__cuda", "11.8" },
                    { "__archspec", "x86_64_v2" },
                };

                auto pkgs = get_virtual_packages("linux-64", overrides);
                REQUIRE(pkgs.back().name == "__cuda");
                REQUIRE(pkgs.back().version == "11.8");
                REQUIRE(pkgs[pkgs.size() - 2].name == "__archspec");
                REQUIRE(pkgs[pkgs.size() - 2].build_string == "x86_64_v2");
            }

            TEST_CASE("override_virtual_packages_env_over_config")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});

                util::set_env("CONDA_OVERRIDE_CUDA", "9.0");
                const override_virtual_packages_map overrides = { { "cuda", "13.1" } };

                auto pkgs = get_virtual_packages("linux-64", overrides);
                REQUIRE(pkgs.back().name == "__cuda");
                REQUIRE(pkgs.back().version == "9.0");
            }

            // Mirrors conda/tests/plugins/test_virtual_packages.py override behavior.
            TEST_CASE("conda_parity_override_from_config_when_env_unset")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});
                const override_virtual_packages_map overrides = { { "cuda", "overridden" } };

                const auto pkgs = get_virtual_packages("linux-64", overrides);
                REQUIRE(require_virtual_package(pkgs, "__cuda").version == "overridden");
            }

            TEST_CASE("conda_parity_env_override_takes_precedence_over_config")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});
                util::set_env("CONDA_OVERRIDE_CUDA", "priority_override");
                const override_virtual_packages_map overrides = { { "cuda", "overridden" } };

                const auto pkgs = get_virtual_packages("linux-64", overrides);
                REQUIRE(require_virtual_package(pkgs, "__cuda").version == "priority_override");
            }

            TEST_CASE("conda_parity_cuda_override")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});
                const auto override_value = GENERATE(as<std::string>{}, "4.5", "");
                util::set_env("CONDA_OVERRIDE_CUDA", override_value);

                const auto pkgs = get_virtual_packages("linux-64", {});
                if (override_value.empty())
                {
                    REQUIRE_FALSE(has_virtual_package(pkgs, "__cuda"));
                }
                else
                {
                    REQUIRE(require_virtual_package(pkgs, "__cuda").version == override_value);
                }
            }

            TEST_CASE("conda_parity_glibc_override")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});
                const auto override_value = GENERATE(as<std::string>{}, "1.0", "");
                util::set_env("CONDA_OVERRIDE_GLIBC", override_value);

                const auto pkgs = get_virtual_packages("linux-64", {});
                REQUIRE(has_virtual_package(pkgs, "__glibc") == !override_value.empty());
                if (!override_value.empty())
                {
                    REQUIRE(require_virtual_package(pkgs, "__glibc").version == override_value);
                }
            }

            TEST_CASE("conda_parity_linux_empty_override_keeps_package")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});
                util::set_env("CONDA_OVERRIDE_LINUX", "");

                const auto pkgs = get_virtual_packages("linux-64", {});
                REQUIRE(has_virtual_package(pkgs, "__linux"));
            }

            TEST_CASE("conda_parity_dunder_config_keys")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});
                const override_virtual_packages_map overrides = { { "__cuda", "11.8" } };

                const auto pkgs = get_virtual_packages("linux-64", overrides);
                REQUIRE(require_virtual_package(pkgs, "__cuda").version == "11.8");
            }

            TEST_CASE("conda_parity_empty_config_cuda_suppresses_package")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});
                const override_virtual_packages_map overrides = { { "cuda", "" } };

                const auto pkgs = get_virtual_packages("linux-64", overrides);
                REQUIRE_FALSE(has_virtual_package(pkgs, "__cuda"));
            }

            TEST_CASE("conda_parity_empty_env_overrides_config")
            {
                mambatests::EnvironmentCleaner env_clean(mambatests::CleanMambaEnv{});
                util::set_env("CONDA_OVERRIDE_CUDA", "");
                const override_virtual_packages_map overrides = { { "cuda", "13.1" } };

                const auto pkgs = get_virtual_packages("linux-64", overrides);
                REQUIRE_FALSE(has_virtual_package(pkgs, "__cuda"));
            }
        }
    }
}
