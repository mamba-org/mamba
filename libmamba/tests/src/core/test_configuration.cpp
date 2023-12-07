// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/api/configuration.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace detail
    {
        bool has_config_name(const std::string& file);

        bool is_config_file(const fs::u8path& path);

        void print_scalar_node(YAML::Emitter&, YAML::Node value, YAML::Node source, bool show_source);
        void print_seq_node(YAML::Emitter&, YAML::Node value, YAML::Node source, bool show_source);
        void print_map_node(YAML::Emitter&, YAML::Node value, YAML::Node source, bool show_source);
    }

    namespace testing
    {
        class Configuration
        {
        public:

            Configuration()
            {
                m_channel_alias_bu = ctx.channel_alias;
                m_ssl_verify = ctx.remote_fetch_params.ssl_verify;
                m_proxy_servers = ctx.remote_fetch_params.proxy_servers;
            }

            ~Configuration()
            {
                config.reset_configurables();
                ctx.channel_alias = m_channel_alias_bu;
                ctx.remote_fetch_params.ssl_verify = m_ssl_verify;
                ctx.remote_fetch_params.proxy_servers = m_proxy_servers;
            }

        protected:

            void load_test_config(std::string rc)
            {
                const auto unique_location = tempfile_ptr->path();
                std::ofstream out_file(
                    unique_location.std_path(),
                    std::ofstream::out | std::ofstream::trunc
                );
                out_file << rc;
                out_file.close();

                config.reset_configurables();
                config.at("rc_files").set_value<std::vector<fs::u8path>>({ unique_location });
                config.load();
            }

            void load_test_config(std::vector<std::string> rcs)
            {
                std::vector<std::unique_ptr<TemporaryFile>> tempfiles;
                std::vector<fs::u8path> sources;

                for (auto rc : rcs)
                {
                    tempfiles.push_back(std::make_unique<TemporaryFile>("mambarc", ".yaml"));
                    fs::u8path loc = tempfiles.back()->path();

                    std::ofstream out_file(loc.std_path());
                    out_file << rc;
                    out_file.close();

                    sources.push_back(loc);
                }

                config.reset_configurables();
                config.at("rc_files").set_value(sources);
                config.load();
            }

            std::string shrink_source(std::size_t position)
            {
                return util::shrink_home(config.valid_sources()[position].string());
            }

            std::unique_ptr<TemporaryFile> tempfile_ptr = std::make_unique<TemporaryFile>(
                "mambarc",
                ".yaml"
            );


            mamba::Context& ctx = mambatests::context();
            mamba::Configuration config{ ctx };

        private:

            // Variables to restore the original COntext state and avoid
            // side effect across the tests. A better solution would be to
            // save and restore the whole context (that requires refactoring
            // of the Context class)
            std::string m_channel_alias_bu;
            std::string m_ssl_verify;
            std::map<std::string, std::string> m_proxy_servers;
            mambatests::EnvironmentCleaner restore = { mambatests::CleanMambaEnv() };
        };

        TEST_SUITE("Configuration")
        {
            TEST_CASE_FIXTURE(Configuration, "target_prefix_options")
            {
                CHECK_EQ(!MAMBA_ALLOW_EXISTING_PREFIX, 0);
                CHECK_EQ(!MAMBA_ALLOW_MISSING_PREFIX, 0);
                CHECK_EQ(!MAMBA_ALLOW_NOT_ENV_PREFIX, 0);
                CHECK_EQ(!MAMBA_EXPECT_EXISTING_PREFIX, 0);

                CHECK_EQ(!MAMBA_ALLOW_EXISTING_PREFIX, MAMBA_NOT_ALLOW_EXISTING_PREFIX);

                CHECK_EQ(
                    MAMBA_NOT_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                        | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX,
                    0
                );
            }

            TEST_CASE_FIXTURE(Configuration, "load_rc_file")
            {
                std::string rc = unindent(R"(
                    channels:
                        - test1)");
                load_test_config(rc);
                const auto src = util::shrink_home(tempfile_ptr->path().string());
                CHECK_EQ(config.sources().size(), 1);
                CHECK_EQ(config.valid_sources().size(), 1);
                CHECK_EQ(config.dump(), "channels:\n  - test1");
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    "channels:\n  - test1  # '" + src + "'"
                );

                // ill-formed config file
                rc = unindent(R"(
                    channels:
                        - test10
                       - https://repo.mamba.pm/conda-forge)");

                load_test_config(rc);

                CHECK_EQ(config.sources().size(), 1);
                CHECK_EQ(config.valid_sources().size(), 0);
                CHECK_EQ(config.dump(), "");
                CHECK_EQ(config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS), "");
            }

            // Regression test for https://github.com/mamba-org/mamba/issues/2934
            TEST_CASE_FIXTURE(Configuration, "parse_condarc")
            {
                std::vector<fs::u8path> possible_rc_paths = {
                    mambatests::test_data_dir / "config/.condarc",
                };

                config.set_rc_values(possible_rc_paths, RCConfigLevel::kTargetPrefix);
            };


            TEST_CASE_FIXTURE(Configuration, "load_rc_files")
            {
                std::string rc1 = unindent(R"(
                    channels:
                        - test1
                    ssl_verify: false)");

                std::string rc2 = unindent(R"(
                    channels:
                        - test2
                        - test1)");

                std::vector<std::string> rcs = { rc1, rc2 };
                load_test_config(rcs);

                REQUIRE_EQ(config.sources().size(), 2);
                REQUIRE_EQ(config.valid_sources().size(), 2);

                std::string src1 = shrink_source(0);
                std::string src2 = shrink_source(1);
                CHECK_EQ(config.dump(), unindent(R"(
                                        channels:
                                          - test1
                                          - test2
                                        ssl_verify: <false>)"));
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                        channels:
                                          - test1  # ')"
                              + src1 + R"('
                                          - test2  # ')"
                              + src2 + R"('
                                        ssl_verify: <false>  # ')"
                              + src1 + "'")
                                 .c_str())
                );

                // ill-formed key
                std::string rc3 = unindent(R"(
                    channels:
                        - test3
                    override_channels_enabled:
                        - false)");
                rcs.push_back(rc3);
                load_test_config(rcs);

                REQUIRE_EQ(config.sources().size(), 3);
                REQUIRE_EQ(config.valid_sources().size(), 3);

                // tmp files changed
                src1 = shrink_source(0);
                src2 = shrink_source(1);
                std::string src3 = shrink_source(2);
                CHECK_EQ(config.dump(), unindent(R"(
                                        channels:
                                          - test1
                                          - test2
                                          - test3
                                        ssl_verify: <false>)"));
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                        channels:
                                          - test1  # ')"
                              + src1 + R"('
                                          - test2  # ')"
                              + src2 + R"('
                                          - test3  # ')"
                              + src3 + R"('
                                        ssl_verify: <false>  # ')"
                              + src1 + "'")
                                 .c_str())
                );

                // ill-formed file
                std::string rc4 = unindent(R"(
                    channels:
                      - test3
                     - test4)");
                rcs.push_back(rc4);
                load_test_config(rcs);

                REQUIRE_EQ(config.sources().size(), 4);
                REQUIRE_EQ(config.valid_sources().size(), 3);

                // tmp files changed
                src1 = shrink_source(0);
                src2 = shrink_source(1);
                src3 = shrink_source(2);
                CHECK_EQ(config.dump(), unindent(R"(
                                        channels:
                                          - test1
                                          - test2
                                          - test3
                                        ssl_verify: <false>)"));
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                        channels:
                                          - test1  # ')"
                              + src1 + R"('
                                          - test2  # ')"
                              + src2 + R"('
                                          - test3  # ')"
                              + src3 + R"('
                                        ssl_verify: <false>  # ')"
                              + src1 + "'")
                                 .c_str())
                );
            }

            TEST_CASE_FIXTURE(Configuration, "dump")
            {
                std::string rc1 = unindent(R"(
                    channels:
                        - test1
                        - https://repo.mamba.pm/conda-forge
                    override_channels_enabled: true
                    allow_softlinks: true
                    test_complex_structure:
                        - foo: bar
                        - bar: baz)");

                std::string rc2 = unindent(R"(
                    channels:
                        - test10
                    override_channels_enabled: false)");

                load_test_config({ rc1, rc2 });

                REQUIRE_EQ(config.sources().size(), 2);
                REQUIRE_EQ(config.valid_sources().size(), 2);
                std::string src1 = shrink_source(0);
                std::string src2 = util::shrink_home(shrink_source(1));

                std::string res = config.dump();
                // Unexpected/handled keys are dropped
                CHECK_EQ(res, unindent(R"(
                                    channels:
                                      - test1
                                      - https://repo.mamba.pm/conda-forge
                                      - test10
                                    override_channels_enabled: true
                                    allow_softlinks: true)"));

                res = config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS);
                CHECK_EQ(
                    res,
                    unindent((R"(
                                    channels:
                                      - test1  # ')"
                              + src1 + R"('
                                      - https://repo.mamba.pm/conda-forge  # ')"
                              + src1 + R"('
                                      - test10  # ')"
                              + src2 + R"('
                                    override_channels_enabled: true  # ')"
                              + src1 + "' > '" + src2 + R"('
                                    allow_softlinks: true  # ')"
                              + src1 + "'")
                                 .c_str())
                );
            }

            TEST_CASE_FIXTURE(Configuration, "channels")
            {
                std::string rc1 = unindent(R"(
                    channels:
                        - c11
                        - c12)");
                std::string rc2 = unindent(R"(
                    channels:
                        - c21
                        - c12)");
                std::string rc3 = unindent(R"(
                    channels:
                        - c11
                        - c32
                        - c21)");
                load_test_config({ rc1, rc2, rc3 });

                CHECK_EQ(config.dump(), unindent(R"(
                                    channels:
                                      - c11
                                      - c12
                                      - c21
                                      - c32)"));

                util::set_env("CONDA_CHANNELS", "c90,c101");
                load_test_config(rc1);

                CHECK_EQ(config.dump(), unindent(R"(
                                    channels:
                                      - c90
                                      - c101
                                      - c11
                                      - c12)"));

                REQUIRE_EQ(config.sources().size(), 1);
                REQUIRE_EQ(config.valid_sources().size(), 1);
                std::string src1 = shrink_source(0);

                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    channels:
                                      - c90  # 'CONDA_CHANNELS'
                                      - c101  # 'CONDA_CHANNELS'
                                      - c11  # ')"
                              + src1 + R"('
                                      - c12  # ')"
                              + src1 + "'")
                                 .c_str())
                );

                config.at("channels").set_yaml_value("https://my.channel, https://my2.channel").compute();
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    channels:
                                      - https://my.channel  # 'API'
                                      - https://my2.channel  # 'API'
                                      - c90  # 'CONDA_CHANNELS'
                                      - c101  # 'CONDA_CHANNELS'
                                      - c11  # ')"
                              + src1 + R"('
                                      - c12  # ')"
                              + src1 + "'")
                                 .c_str())
                );
                CHECK_EQ(ctx.channels, config.at("channels").value<std::vector<std::string>>());

                util::unset_env("CONDA_CHANNELS");
            }

            TEST_CASE_FIXTURE(Configuration, "default_channels")
            {
                std::string rc1 = unindent(R"(
                    default_channels:
                      - c11
                      - c12)");
                std::string rc2 = unindent(R"(
                    default_channels:
                      - c21
                      - c12)");
                std::string rc3 = unindent(R"(
                    default_channels:
                      - c11
                      - c32
                      - c21)");
                load_test_config({ rc1, rc2, rc3 });

                CHECK_EQ(config.dump(), unindent(R"(
                            default_channels:
                              - c11
                              - c12
                              - c21
                              - c32)"));

                util::set_env("MAMBA_DEFAULT_CHANNELS", "c91,c100");
                load_test_config(rc1);

                CHECK_EQ(config.dump(), unindent(R"(
                                    default_channels:
                                      - c91
                                      - c100
                                      - c11
                                      - c12)"));

                REQUIRE_EQ(config.sources().size(), 1);
                REQUIRE_EQ(config.valid_sources().size(), 1);
                std::string src1 = shrink_source(0);

                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    default_channels:
                                      - c91  # 'MAMBA_DEFAULT_CHANNELS'
                                      - c100  # 'MAMBA_DEFAULT_CHANNELS'
                                      - c11  # ')"
                              + src1 + R"('
                                      - c12  # ')"
                              + src1 + "'")
                                 .c_str())
                );

                config.at("default_channels")
                    .set_yaml_value("https://my.channel, https://my2.channel")
                    .compute();
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    default_channels:
                                      - https://my.channel  # 'API'
                                      - https://my2.channel  # 'API'
                                      - c91  # 'MAMBA_DEFAULT_CHANNELS'
                                      - c100  # 'MAMBA_DEFAULT_CHANNELS'
                                      - c11  # ')"
                              + src1 + R"('
                                      - c12  # ')"
                              + src1 + "'")
                                 .c_str())
                );
                CHECK_EQ(
                    ctx.default_channels,
                    config.at("default_channels").value<std::vector<std::string>>()
                );

                util::unset_env("MAMBA_DEFAULT_CHANNELS");
            }

            TEST_CASE_FIXTURE(Configuration, "channel_alias")
            {
                std::string rc1 = "channel_alias: http://repo.mamba.pm/";
                std::string rc2 = "channel_alias: https://conda.anaconda.org/";

                load_test_config({ rc1, rc2 });
                CHECK_EQ(config.dump(), "channel_alias: http://repo.mamba.pm/");

                load_test_config({ rc2, rc1 });
                CHECK_EQ(config.dump(), "channel_alias: https://conda.anaconda.org/");

                util::set_env("MAMBA_CHANNEL_ALIAS", "https://foo.bar");
                load_test_config(rc1);

                CHECK_EQ(config.dump(), "channel_alias: https://foo.bar");

                REQUIRE_EQ(config.sources().size(), 1);
                REQUIRE_EQ(config.valid_sources().size(), 1);
                std::string src1 = shrink_source(0);

                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    "channel_alias: https://foo.bar  # 'MAMBA_CHANNEL_ALIAS' > '" + src1 + "'"
                );

                config.at("channel_alias").set_yaml_value("https://my.channel").compute();
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    "channel_alias: https://my.channel  # 'API' > 'MAMBA_CHANNEL_ALIAS' > '" + src1 + "'"
                );
                CHECK_EQ(ctx.channel_alias, config.at("channel_alias").value<std::string>());

                util::unset_env("MAMBA_CHANNEL_ALIAS");
            }

            TEST_CASE_FIXTURE(Configuration, "pkgs_dirs")
            {
                std::string cache1 = util::path_concat(util::user_home_dir(), "foo");
                std::string cache2 = util::path_concat(util::user_home_dir(), "bar");

                std::string rc1 = "pkgs_dirs:\n  - " + cache1;
                std::string rc2 = "pkgs_dirs:\n  - " + cache2;

                load_test_config({ rc1, rc2 });
                CHECK_EQ(config.dump(), "pkgs_dirs:\n  - " + cache1 + "\n  - " + cache2);

                load_test_config({ rc2, rc1 });
                CHECK_EQ(config.dump(), "pkgs_dirs:\n  - " + cache2 + "\n  - " + cache1);

                std::string cache3 = util::path_concat(util::user_home_dir(), "baz");
                util::set_env("CONDA_PKGS_DIRS", cache3);
                load_test_config(rc1);
                CHECK_EQ(config.dump(), "pkgs_dirs:\n  - " + cache3 + "\n  - " + cache1);

                REQUIRE_EQ(config.sources().size(), 1);
                REQUIRE_EQ(config.valid_sources().size(), 1);
                std::string src1 = shrink_source(0);

                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    pkgs_dirs:
                                      - )"
                              + cache3 + R"(  # 'CONDA_PKGS_DIRS'
                                      - )"
                              + cache1 + "  # '" + src1 + "'")
                                 .c_str())
                );

                util::unset_env("CONDA_PKGS_DIRS");

                std::string empty_rc = "";
                std::string root_prefix_str = util::path_concat(util::user_home_dir(), "any_prefix");
                util::set_env("MAMBA_ROOT_PREFIX", root_prefix_str);
                load_test_config(empty_rc);

#ifdef _WIN32
                std::string extra_cache = "\n  - "
                                          + (fs::u8path(util::get_env("APPDATA").value_or(""))
                                             / ".mamba" / "pkgs")
                                                .string()
                                          + "  # 'fallback'";
#else
                std::string extra_cache = "";
#endif
                CHECK_EQ(
                    config.dump(
                        MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS | MAMBA_SHOW_ALL_CONFIGS,
                        { "pkgs_dirs" }
                    ),
                    unindent((R"(
                                    pkgs_dirs:
                                      - )"
                              + (fs::u8path(root_prefix_str) / "pkgs").string() + R"(  # 'fallback'
                                      - )"
                              + (fs::u8path(util::user_home_dir()) / ".mamba" / "pkgs").string()
                              + R"(  # 'fallback')" + extra_cache)
                                 .c_str())
                );
                CHECK_EQ(ctx.pkgs_dirs, config.at("pkgs_dirs").value<std::vector<fs::u8path>>());

                std::string cache4 = util::path_concat(util::user_home_dir(), "babaz");
                util::set_env("CONDA_PKGS_DIRS", cache4);
                load_test_config(empty_rc);
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    pkgs_dirs:
                                      - )"
                              + cache4 + "  # 'CONDA_PKGS_DIRS'")
                                 .c_str())
                );

                util::unset_env("CONDA_PKGS_DIRS");
                util::unset_env("MAMBA_ROOT_PREFIX");
                config.clear_values();
            }

            TEST_CASE_FIXTURE(Configuration, "ssl_verify")
            {
                // Default empty string value
                ctx.remote_fetch_params.ssl_verify = "";
                std::string rc = "";
                load_test_config(rc);
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "<system>");

                rc = "ssl_verify: true";
                load_test_config(rc);
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "<system>");

                rc = "ssl_verify: <true>";
                load_test_config(rc);
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "<system>");

                rc = "ssl_verify: 1";
                load_test_config(rc);
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "<system>");

                rc = "ssl_verify: 10";
                load_test_config(rc);
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "10");

                rc = "ssl_verify: false";
                load_test_config(rc);
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "<false>");

                rc = "ssl_verify: <false>";
                load_test_config(rc);
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "<false>");

                rc = "ssl_verify: 0";
                load_test_config(rc);
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "<false>");

                rc = "ssl_verify: /foo/bar/baz";
                load_test_config(rc);
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "/foo/bar/baz");

                std::string rc1 = "ssl_verify: true";
                std::string rc2 = "ssl_verify: false";
                load_test_config({ rc1, rc2 });
                CHECK_EQ(config.at("ssl_verify").value<std::string>(), "<system>");
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "<system>");

                load_test_config({ rc2, rc1 });
                CHECK_EQ(config.at("ssl_verify").value<std::string>(), "<false>");
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "<false>");

                util::set_env("MAMBA_SSL_VERIFY", "/env/bar/baz");
                load_test_config(rc1);

                REQUIRE_EQ(config.sources().size(), 1);
                REQUIRE_EQ(config.valid_sources().size(), 1);
                std::string src1 = shrink_source(0);

                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    "ssl_verify: /env/bar/baz  # 'MAMBA_SSL_VERIFY' > '" + src1 + "'"
                );

                config.at("ssl_verify").set_yaml_value("/new/test").compute();
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    "ssl_verify: /new/test  # 'API' > 'MAMBA_SSL_VERIFY' > '" + src1 + "'"
                );

                util::unset_env("MAMBA_SSL_VERIFY");
            }
#undef EXPECT_CA_EQUAL

            TEST_CASE_FIXTURE(Configuration, "cacert_path")
            {
                std::string rc = "ssl_verify: /foo/bar/baz\ncacert_path: /other/foo/bar/baz";
                load_test_config(rc);
                CHECK_EQ(config.at("ssl_verify").value<std::string>(), "/other/foo/bar/baz");
                CHECK_EQ(config.at("cacert_path").value<std::string>(), "/other/foo/bar/baz");
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "/other/foo/bar/baz");

                util::set_env("MAMBA_CACERT_PATH", "/env/ca/baz");
                load_test_config(rc);

                REQUIRE_EQ(config.sources().size(), 1);
                REQUIRE_EQ(config.valid_sources().size(), 1);
                std::string src = shrink_source(0);

                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    cacert_path: /env/ca/baz  # 'MAMBA_CACERT_PATH' > ')"
                              + src + R"('
                                    ssl_verify: /env/ca/baz  # ')"
                              + src + "'")
                                 .c_str())
                );
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "/env/ca/baz");

                config.at("cacert_path").set_yaml_value("/new/test").compute();
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    cacert_path: /new/test  # 'API' > 'MAMBA_CACERT_PATH' > ')"
                              + src + R"('
                                    ssl_verify: /env/ca/baz  # ')"
                              + src + "'")
                                 .c_str())
                );
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "/env/ca/baz");

                config.at("ssl_verify").compute();
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    cacert_path: /new/test  # 'API' > 'MAMBA_CACERT_PATH' > ')"
                              + src + R"('
                                    ssl_verify: /new/test  # ')"
                              + src + "'")
                                 .c_str())
                );
                CHECK_EQ(ctx.remote_fetch_params.ssl_verify, "/new/test");

                util::unset_env("MAMBA_CACERT_PATH");
                load_test_config("cacert_path:\nssl_verify: true");  // reset ssl verify to default
            }

            TEST_CASE_FIXTURE(Configuration, "proxy_servers")
            {
                std::string rc = unindent(R"(
                    proxy_servers:
                        http: foo
                        https: bar)");
                load_test_config(rc);
                auto& actual = config.at("proxy_servers").value<std::map<std::string, std::string>>();
                std::map<std::string, std::string> expected = { { "http", "foo" },
                                                                { "https", "bar" } };
                CHECK_EQ(actual, expected);
                CHECK_EQ(ctx.remote_fetch_params.proxy_servers, expected);

                CHECK_EQ(config.sources().size(), 1);
                CHECK_EQ(config.valid_sources().size(), 1);
                CHECK_EQ(config.dump(), "proxy_servers:\n  http: foo\n  https: bar");
            }

            TEST_CASE_FIXTURE(Configuration, "platform")
            {
                CHECK_EQ(ctx.platform, ctx.host_platform);

                std::string rc = "platform: mylinux-128";
                load_test_config(rc);
                std::string src = shrink_source(0);
                CHECK_EQ(config.at("platform").value<std::string>(), "mylinux-128");
                CHECK_EQ(ctx.platform, "mylinux-128");
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    platform: mylinux-128  # ')"
                              + src + "'")
                                 .c_str())
                );

                util::set_env("CONDA_SUBDIR", "win-32");
                load_test_config(rc);
                src = shrink_source(0);
                CHECK_EQ(config.at("platform").value<std::string>(), "win-32");
                CHECK_EQ(ctx.platform, "win-32");
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    platform: win-32  # 'CONDA_SUBDIR' > ')"
                              + src + "'")
                                 .c_str())
                );

                config.at("platform").clear_values();
                ctx.platform = ctx.host_platform;
            }

#define TEST_BOOL_CONFIGURABLE(NAME, CTX)                                                           \
    TEST_CASE_FIXTURE(Configuration, #NAME)                                                         \
    {                                                                                               \
        std::string rc1 = std::string(#NAME) + ": true";                                            \
        std::string rc2 = std::string(#NAME) + ": false";                                           \
        if (config.at(#NAME).rc_configurable())                                                     \
        {                                                                                           \
            load_test_config({ rc1, rc2 });                                                         \
            CHECK(config.at(#NAME).value<bool>());                                                  \
            CHECK(CTX);                                                                             \
                                                                                                    \
            load_test_config({ rc2, rc1 });                                                         \
            CHECK_FALSE(config.at(#NAME).value<bool>());                                            \
            CHECK_FALSE(CTX);                                                                       \
        }                                                                                           \
                                                                                                    \
        std::string env_name = "MAMBA_" + util::to_upper(#NAME);                                    \
        util::set_env(env_name, "true");                                                            \
        load_test_config(rc2);                                                                      \
                                                                                                    \
        REQUIRE_EQ(config.sources().size(), 1);                                                     \
        REQUIRE_EQ(config.valid_sources().size(), 1);                                               \
        std::string src = shrink_source(0);                                                         \
                                                                                                    \
        std::string expected;                                                                       \
        if (config.at(#NAME).rc_configurable())                                                     \
        {                                                                                           \
            expected = std::string(#NAME) + ": true  # '" + env_name + "' > '" + src + "'";         \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            expected = std::string(#NAME) + ": true  # '" + env_name + "'";                         \
        }                                                                                           \
        int dump_opts = MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS;                          \
        CHECK_EQ((config.dump(dump_opts, { #NAME })), expected);                                    \
        CHECK(config.at(#NAME).value<bool>());                                                      \
        CHECK(CTX);                                                                                 \
                                                                                                    \
        if (config.at(#NAME).rc_configurable())                                                     \
        {                                                                                           \
            expected = std::string(#NAME) + ": true  # 'API' > '" + env_name + "' > '" + src + "'"; \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            expected = std::string(#NAME) + ": true  # 'API' > '" + env_name + "'";                 \
        }                                                                                           \
        config.at(#NAME).set_yaml_value("true").compute();                                          \
        CHECK_EQ((config.dump(dump_opts, { #NAME })), expected);                                    \
        CHECK(config.at(#NAME).value<bool>());                                                      \
        CHECK(CTX);                                                                                 \
                                                                                                    \
        util::set_env(env_name, "yeap");                                                            \
        REQUIRE_THROWS_AS(load_test_config(rc2), YAML::Exception);                                  \
                                                                                                    \
        util::unset_env(env_name);                                                                  \
        load_test_config(rc2);                                                                      \
    }

            TEST_BOOL_CONFIGURABLE(ssl_no_revoke, ctx.remote_fetch_params.ssl_no_revoke);

            TEST_BOOL_CONFIGURABLE(override_channels_enabled, ctx.override_channels_enabled);

            TEST_BOOL_CONFIGURABLE(auto_activate_base, ctx.auto_activate_base);

            TEST_CASE_FIXTURE(Configuration, "channel_priority")
            {
                std::string rc1 = "channel_priority: flexible";
                std::string rc2 = "channel_priority: strict";
                std::string rc3 = "channel_priority: disabled";

                load_test_config({ rc1, rc2, rc3 });
                CHECK_EQ(
                    config.at("channel_priority").value<ChannelPriority>(),
                    ChannelPriority::Flexible
                );
                CHECK(ctx.channel_priority == ChannelPriority::Flexible);

                load_test_config({ rc3, rc1, rc2 });
                CHECK_EQ(
                    config.at("channel_priority").value<ChannelPriority>(),
                    ChannelPriority::Disabled
                );
                CHECK(ctx.channel_priority == ChannelPriority::Disabled);

                load_test_config({ rc2, rc1, rc3 });
                CHECK_EQ(config.at("channel_priority").value<ChannelPriority>(), ChannelPriority::Strict);
                CHECK(ctx.channel_priority == ChannelPriority::Strict);

                util::set_env("MAMBA_CHANNEL_PRIORITY", "strict");
                load_test_config(rc3);

                REQUIRE_EQ(config.sources().size(), 1);
                REQUIRE_EQ(config.valid_sources().size(), 1);
                std::string src = shrink_source(0);

                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    "channel_priority: strict  # 'MAMBA_CHANNEL_PRIORITY' > '" + src + "'"
                );
                CHECK_EQ(config.at("channel_priority").value<ChannelPriority>(), ChannelPriority::Strict);
                CHECK_EQ(ctx.channel_priority, ChannelPriority::Strict);

                config.at("channel_priority").set_yaml_value("flexible").compute();
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    "channel_priority: flexible  # 'API' > 'MAMBA_CHANNEL_PRIORITY' > '" + src + "'"
                );
                CHECK_EQ(
                    config.at("channel_priority").value<ChannelPriority>(),
                    ChannelPriority::Flexible
                );
                CHECK_EQ(ctx.channel_priority, ChannelPriority::Flexible);

                util::set_env("MAMBA_CHANNEL_PRIORITY", "stric");
                REQUIRE_THROWS_AS(load_test_config(rc3), YAML::Exception);

                util::unset_env("MAMBA_CHANNEL_PRIORITY");
            }

            TEST_CASE_FIXTURE(Configuration, "pinned_packages")
            {
                std::string rc1 = unindent(R"(
                    pinned_packages:
                        - jupyterlab=3
                        - numpy=1.19)");
                std::string rc2 = unindent(R"(
                    pinned_packages:
                        - matplotlib
                        - numpy=1.19)");
                std::string rc3 = unindent(R"(
                    pinned_packages:
                        - jupyterlab=3
                        - bokeh
                        - matplotlib)");

                load_test_config({ rc1, rc2, rc3 });
                CHECK_EQ(config.dump(), unindent(R"(
                                            pinned_packages:
                                              - jupyterlab=3
                                              - numpy=1.19
                                              - matplotlib
                                              - bokeh)"));
                CHECK_EQ(
                    ctx.pinned_packages,
                    std::vector<std::string>({ "jupyterlab=3", "numpy=1.19", "matplotlib", "bokeh" })
                );

                load_test_config({ rc2, rc1, rc3 });
                REQUIRE(config.at("pinned_packages").yaml_value());
                CHECK_EQ(config.dump(), unindent(R"(
                                            pinned_packages:
                                              - matplotlib
                                              - numpy=1.19
                                              - jupyterlab=3
                                              - bokeh)"));
                CHECK_EQ(
                    ctx.pinned_packages,
                    std::vector<std::string>({ "matplotlib", "numpy=1.19", "jupyterlab=3", "bokeh" })
                );

                util::set_env("MAMBA_PINNED_PACKAGES", "mpl=10.2,xtensor");
                load_test_config(rc1);
                REQUIRE_EQ(config.sources().size(), 1);
                REQUIRE_EQ(config.valid_sources().size(), 1);
                std::string src1 = shrink_source(0);

                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    pinned_packages:
                                      - mpl=10.2  # 'MAMBA_PINNED_PACKAGES'
                                      - xtensor  # 'MAMBA_PINNED_PACKAGES'
                                      - jupyterlab=3  # ')"
                              + src1 + R"('
                                      - numpy=1.19  # ')"
                              + src1 + "'")
                                 .c_str())
                );
                CHECK_EQ(
                    ctx.pinned_packages,
                    std::vector<std::string>({ "mpl=10.2", "xtensor", "jupyterlab=3", "numpy=1.19" })
                );

                config.at("pinned_packages").set_yaml_value("pytest").compute();
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    unindent((R"(
                                    pinned_packages:
                                      - pytest  # 'API'
                                      - mpl=10.2  # 'MAMBA_PINNED_PACKAGES'
                                      - xtensor  # 'MAMBA_PINNED_PACKAGES'
                                      - jupyterlab=3  # ')"
                              + src1 + R"('
                                      - numpy=1.19  # ')"
                              + src1 + "'")
                                 .c_str())
                );
                CHECK_EQ(
                    ctx.pinned_packages,
                    std::vector<std::string>(
                        { "pytest", "mpl=10.2", "xtensor", "jupyterlab=3", "numpy=1.19" }
                    )
                );

                util::unset_env("MAMBA_PINNED_PACKAGES");
            }


            TEST_BOOL_CONFIGURABLE(no_pin, config.at("no_pin").value<bool>());

            TEST_BOOL_CONFIGURABLE(retry_clean_cache, config.at("retry_clean_cache").value<bool>());

            TEST_BOOL_CONFIGURABLE(allow_softlinks, ctx.allow_softlinks);

            TEST_BOOL_CONFIGURABLE(always_softlink, ctx.always_softlink);

            TEST_BOOL_CONFIGURABLE(always_copy, ctx.always_copy);

            TEST_CASE_FIXTURE(Configuration, "always_softlink_and_copy")
            {
                util::set_env("MAMBA_ALWAYS_COPY", "true");
                REQUIRE_THROWS_AS(load_test_config("always_softlink: true"), std::runtime_error);
                util::unset_env("MAMBA_ALWAYS_COPY");

                util::set_env("MAMBA_ALWAYS_SOFTLINK", "true");
                REQUIRE_THROWS_AS(load_test_config("always_copy: true"), std::runtime_error);
                util::unset_env("MAMBA_ALWAYS_SOFTLINK");

                load_test_config("always_softlink: false\nalways_copy: false");
            }

            TEST_CASE_FIXTURE(Configuration, "safety_checks")
            {
                std::string rc1 = "safety_checks: enabled";
                std::string rc2 = "safety_checks: warn";
                std::string rc3 = "safety_checks: disabled";

                load_test_config({ rc1, rc2, rc3 });
                CHECK_EQ(
                    config.at("safety_checks").value<VerificationLevel>(),
                    VerificationLevel::Enabled
                );
                CHECK_EQ(ctx.validation_params.safety_checks, VerificationLevel::Enabled);

                load_test_config({ rc2, rc1, rc3 });
                CHECK_EQ(config.at("safety_checks").value<VerificationLevel>(), VerificationLevel::Warn);
                CHECK_EQ(ctx.validation_params.safety_checks, VerificationLevel::Warn);

                load_test_config({ rc3, rc1, rc3 });
                CHECK_EQ(
                    config.at("safety_checks").value<VerificationLevel>(),
                    VerificationLevel::Disabled
                );
                CHECK_EQ(ctx.validation_params.safety_checks, VerificationLevel::Disabled);

                util::set_env("MAMBA_SAFETY_CHECKS", "warn");
                load_test_config(rc1);

                REQUIRE_EQ(config.sources().size(), 1);
                REQUIRE_EQ(config.valid_sources().size(), 1);
                std::string src = shrink_source(0);

                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    "safety_checks: warn  # 'MAMBA_SAFETY_CHECKS' > '" + src + "'"
                );
                CHECK_EQ(config.at("safety_checks").value<VerificationLevel>(), VerificationLevel::Warn);
                CHECK_EQ(ctx.validation_params.safety_checks, VerificationLevel::Warn);

                config.at("safety_checks").set_yaml_value("disabled").compute();
                CHECK_EQ(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS),
                    "safety_checks: disabled  # 'API' > 'MAMBA_SAFETY_CHECKS' > '" + src + "'"
                );
                CHECK_EQ(
                    config.at("safety_checks").value<VerificationLevel>(),
                    VerificationLevel::Disabled
                );
                CHECK_EQ(ctx.validation_params.safety_checks, VerificationLevel::Disabled);

                util::set_env("MAMBA_SAFETY_CHECKS", "yeap");
                REQUIRE_THROWS_AS(load_test_config(rc2), std::runtime_error);

                util::unset_env("MAMBA_SAFETY_CHECKS");
                load_test_config(rc2);
            }

            TEST_BOOL_CONFIGURABLE(extra_safety_checks, ctx.validation_params.extra_safety_checks);

#undef TEST_BOOL_CONFIGURABLE

            TEST_CASE_FIXTURE(Configuration, "has_config_name")
            {
                using namespace detail;

                CHECK_FALSE(has_config_name(""));
                CHECK_FALSE(has_config_name("conf"));
                CHECK_FALSE(has_config_name("config"));
                CHECK_FALSE(has_config_name("config.conda"));
                CHECK_FALSE(has_config_name("conf.condarc"));
                CHECK_FALSE(has_config_name("conf.mambarc"));

                CHECK(has_config_name("condarc"));
                CHECK(has_config_name("mambarc"));
                CHECK(has_config_name(".condarc"));
                CHECK(has_config_name(".mambarc"));
                CHECK(has_config_name(".yaml"));
                CHECK(has_config_name(".yml"));
                CHECK(has_config_name("conf.yaml"));
                CHECK(has_config_name("config.yml"));
            }

            TEST_CASE_FIXTURE(Configuration, "is_config_file")
            {
                using namespace detail;

                fs::u8path p = mambatests::test_data_dir / "config/.condarc";

                std::vector<fs::u8path> wrong_paths = {
                    mambatests::test_data_dir / "config",
                    mambatests::test_data_dir / "conf",
                    mambatests::test_data_dir / "config/condarc",
                    mambatests::test_data_dir / "history/conda-meta/history",
                };

                CHECK(is_config_file(p));

                for (const fs::u8path& wp : wrong_paths)
                {
                    CHECK_FALSE(is_config_file(wp));
                }
            }

            // Regression test for https://github.com/mamba-org/mamba/issues/2704
            TEST_CASE_FIXTURE(Configuration, "deduplicate_rc_files")
            {
                using namespace detail;

                std::vector<fs::u8path> sources;

                auto temp_prefix = std::make_unique<TemporaryDirectory>();
                auto temp_home = std::make_unique<TemporaryDirectory>();

                util::set_env("MAMBA_ROOT_PREFIX", temp_prefix->path().string());

                // the target_prefix is the same as the root_prefix for the base env
                util::set_env("MAMBA_TARGET_PREFIX", temp_prefix->path().string());
                util::set_env("HOME", temp_home->path().string());
                util::set_env("USERPROFILE", temp_home->path().string());

                auto root_config_file = temp_prefix->path() / ".condarc";
                std::ofstream out_root_config(root_config_file.std_path());
                out_root_config << "channel_alias: http://outer.com\n";
                out_root_config.close();

                auto user_config_file = temp_home->path() / ".condarc";
                std::ofstream out_user_config(user_config_file.std_path());
                out_user_config << "channel_alias: http://inner.com\n";
                out_user_config.close();

                config.load();

                REQUIRE_EQ(config.sources().size(), 2);
                REQUIRE_EQ(config.at("channel_alias").value<std::string>(), "http://inner.com");
            }

            TEST_CASE_FIXTURE(Configuration, "print_scalar_node")
            {
                using namespace detail;

                std::string rc = "foo";
                auto node = YAML::Load(rc);
                auto node_src = YAML::Load("/some/source1");
                YAML::Emitter out;
                print_scalar_node(out, node, node_src, true);

                std::string res = out.c_str();
                CHECK_EQ(res, "foo  # '/some/source1'");

                // These tests do not really make sense since
                // print_scalar should be called by print_configurable only
                // and the check is already done in it.
                /*
                rc = unindent(R"(
                                foo: bar
                                bar: baz)");
                node = YAML::Load(rc);
                CHECK_THROWS_AS(print_scalar_node(out, node, node_src, true), std::runtime_error);

                rc = unindent(R"(
                                - foo
                                - bar)");
                node = YAML::Load(rc);
                CHECK_THROWS_AS(print_scalar_node(out, node, node_src, true), std::runtime_error);

                node = YAML::Node();
                CHECK_THROWS_AS(print_scalar_node(out, node, node_src, true), std::runtime_error);
                */
            }

            TEST_CASE_FIXTURE(Configuration, "print_map_node")
            {
                using namespace detail;

                std::string rc = unindent(R"(
                                    foo: bar
                                    bar: baz)");
                auto node = YAML::Load(rc);
                auto node_src = YAML::Load(unindent(R"(
                                              foo: /some/source1
                                              bar: /some/source2)"));
                YAML::Emitter out;
                print_map_node(out, node, node_src, true);

                std::string res = out.c_str();
                CHECK_EQ(res, unindent(R"(
                                    foo: bar  # '/some/source1'
                                    bar: baz  # '/some/source2')"));

                // These tests do not really make sense since
                // print_scalar should be called by print_configurable only
                // and the check is already done in it.
                /*rc = "foo";
                node = YAML::Load(rc);
                CHECK_THROWS_AS(print_map_node(out, node, node_src, true), std::runtime_error);

                rc = unindent(R"(
                                - foo
                                - bar)");
                node = YAML::Load(rc);
                CHECK_THROWS_AS(print_map_node(out, node, node_src, true), std::runtime_error);

                node = YAML::Node();
                CHECK_THROWS_AS(print_map_node(out, node, node_src, true), std::runtime_error);*/
            }

            TEST_CASE_FIXTURE(Configuration, "print_seq_node")
            {
                using namespace detail;

                std::string rc = unindent(R"(
                                            - foo
                                            - bar
                                            )");
                auto node = YAML::Load(rc);
                auto node_src = YAML::Load(unindent(R"(
                                                    - /some/source1
                                                    - /some/source2
                                                    )"));
                YAML::Emitter out;
                print_seq_node(out, node, node_src, true);

                std::string res = out.c_str();
                CHECK_EQ(res, unindent(R"(
                                      - foo  # '/some/source1'
                                      - bar  # '/some/source2')"));

                // These tests do not really make sense since
                // print_scalar should be called by print_configurable only
                // and the check is already done in it.
                /*rc = "foo";
                node = YAML::Load(rc);
                CHECK_THROWS_AS(print_seq_node(out, node, node_src, true), std::runtime_error);

                rc = unindent(R"(
                                foo: bar
                                bar: baz)");
                node = YAML::Load(rc);
                CHECK_THROWS_AS(print_seq_node(out, node, node_src, true), std::runtime_error);

                node = YAML::Node();
                CHECK_THROWS_AS(print_seq_node(out, node, node_src, true), std::runtime_error);*/
            }
        }
    }  // namespace testing
}  // namespace mamba
