// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

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

            void load_file_specs_config(std::string file_specs)
            {
                const auto unique_location = tempfile_specs_ptr->path();
                std::ofstream out_file(
                    unique_location.std_path(),
                    std::ofstream::out | std::ofstream::trunc
                );
                out_file << file_specs;
                out_file.close();

                config.reset_configurables();
                config.at("file_specs").set_value<std::vector<std::string>>({ unique_location.string() });
                config.load();
            }

            std::string shrink_source(std::size_t position)
            {
                return util::shrink_home(config.valid_sources()[position].string());
            }

            const std::string get_root_prefix_envs_dir()
            {
                return util::path_concat(
                    std::string(config.at("root_prefix").value<mamba::fs::u8path>()),
                    "envs"
                );
            }

            std::unique_ptr<TemporaryFile> tempfile_ptr = std::make_unique<TemporaryFile>(
                "mambarc",
                ".yaml"
            );

            std::unique_ptr<TemporaryFile> tempfile_specs_ptr = std::make_unique<TemporaryFile>(
                "file_specs",
                ".yaml"
            );

            mamba::Context& ctx = mambatests::context();
            mamba::Configuration config{ ctx };

        private:

            // Variables to restore the original Context state and avoid
            // side effect across the tests. A better solution would be to
            // save and restore the whole context (that requires refactoring
            // of the Context class)
            std::string m_channel_alias_bu;
            std::string m_ssl_verify;
            std::map<std::string, std::string> m_proxy_servers;
            mambatests::EnvironmentCleaner restore = { mambatests::CleanMambaEnv() };
        };

        namespace
        {
            TEST_CASE_METHOD(Configuration, "target_prefix_options")
            {
                REQUIRE((!MAMBA_ALLOW_EXISTING_PREFIX) == 0);
                REQUIRE((!MAMBA_ALLOW_MISSING_PREFIX) == 0);
                REQUIRE((!MAMBA_ALLOW_NOT_ENV_PREFIX) == 0);
                REQUIRE((!MAMBA_EXPECT_EXISTING_PREFIX) == 0);

                REQUIRE((!MAMBA_ALLOW_EXISTING_PREFIX) == MAMBA_NOT_ALLOW_EXISTING_PREFIX);

                REQUIRE(
                    (MAMBA_NOT_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                     | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX)
                    == 0
                );
            }

            TEST_CASE_METHOD(Configuration, "load_rc_file")
            {
                std::string rc = unindent(R"(
                    channels:
                        - test1)");
                load_test_config(rc);
                const auto src = util::shrink_home(tempfile_ptr->path().string());
                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                REQUIRE(config.dump() == "channels:\n  - test1");
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == "channels:\n  - test1  # '" + src + "'"
                );

                // ill-formed config file
                rc = unindent(R"(
                    channels:
                        - test10
                       - https://repo.mamba.pm/conda-forge)");

                load_test_config(rc);

                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 0);
                REQUIRE(config.dump() == "");
                REQUIRE(config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS) == "");
            }

            // Regression test for https://github.com/mamba-org/mamba/issues/2934
            TEST_CASE_METHOD(Configuration, "parse_condarc")
            {
                std::vector<fs::u8path> possible_rc_paths = {
                    mambatests::test_data_dir / "config/.condarc",
                };

                config.set_rc_values(possible_rc_paths, RCConfigLevel::kTargetPrefix);
            };

            TEST_CASE_METHOD(Configuration, "load_rc_files")
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

                REQUIRE(config.sources().size() == 2);
                REQUIRE(config.valid_sources().size() == 2);

                std::string src1 = shrink_source(0);
                std::string src2 = shrink_source(1);
                REQUIRE(config.dump() == unindent(R"(
                                        channels:
                                          - test1
                                          - test2
                                        ssl_verify: <false>)"));
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
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

                REQUIRE(config.sources().size() == 3);
                REQUIRE(config.valid_sources().size() == 3);

                // tmp files changed
                src1 = shrink_source(0);
                src2 = shrink_source(1);
                std::string src3 = shrink_source(2);
                REQUIRE(config.dump() == unindent(R"(
                                        channels:
                                          - test1
                                          - test2
                                          - test3
                                        ssl_verify: <false>)"));
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
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

                REQUIRE(config.sources().size() == 4);
                REQUIRE(config.valid_sources().size() == 3);

                // tmp files changed
                src1 = shrink_source(0);
                src2 = shrink_source(1);
                src3 = shrink_source(2);
                REQUIRE(config.dump() == unindent(R"(
                                        channels:
                                          - test1
                                          - test2
                                          - test3
                                        ssl_verify: <false>)"));
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
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

            TEST_CASE_METHOD(Configuration, "load_file_specs")
            {
                std::string file_specs = unindent(R"(
                    name: env_name
                    channels:
                    - https://private.cloud/t/$SOME_PRIVATE_KEY/get/channel
                    - https://private.cloud/t/${SOME_OTHER_PRIVATE_KEY}/get/channel
                    - https://private.cloud/t/SOME_TOKEN/get/channel
                    - conda-forge
                    dependencies:
                    - spec1)");
                util::set_env("SOME_PRIVATE_KEY", "hdfd5256h6degd5");
                util::set_env("SOME_OTHER_PRIVATE_KEY", "kqf458r1h127de9");
                load_file_specs_config(file_specs);
                const auto src = util::shrink_home(tempfile_ptr->path().string());
                REQUIRE(
                    config.dump()
                    == "channels:\n  - https://private.cloud/t/hdfd5256h6degd5/get/channel\n  - https://private.cloud/t/kqf458r1h127de9/get/channel\n  - https://private.cloud/t/SOME_TOKEN/get/channel\n  - conda-forge"
                );
            }

            TEST_CASE_METHOD(Configuration, "dump")
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

                REQUIRE(config.sources().size() == 2);
                REQUIRE(config.valid_sources().size() == 2);
                std::string src1 = shrink_source(0);
                std::string src2 = util::shrink_home(shrink_source(1));

                std::string res = config.dump();
                // Unexpected/handled keys are dropped
                REQUIRE(res == unindent(R"(
                                    channels:
                                      - test1
                                      - https://repo.mamba.pm/conda-forge
                                      - test10
                                    override_channels_enabled: true
                                    allow_softlinks: true)"));

                res = config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS);
                REQUIRE(
                    res
                    == unindent((R"(
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

            TEST_CASE_METHOD(Configuration, "channels")
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

                REQUIRE(config.dump() == unindent(R"(
                                    channels:
                                      - c11
                                      - c12
                                      - c21
                                      - c32)"));

                util::set_env("CONDA_CHANNELS", "c90,c101");
                load_test_config(rc1);

                REQUIRE(config.dump() == unindent(R"(
                                    channels:
                                      - c90
                                      - c101
                                      - c11
                                      - c12)"));

                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                std::string src1 = shrink_source(0);

                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
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
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
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
                REQUIRE(ctx.channels == config.at("channels").value<std::vector<std::string>>());

                util::unset_env("CONDA_CHANNELS");
            }

            TEST_CASE_METHOD(Configuration, "default_channels")
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

                REQUIRE(config.dump() == unindent(R"(
                            default_channels:
                              - c11
                              - c12
                              - c21
                              - c32)"));

                util::set_env("MAMBA_DEFAULT_CHANNELS", "c91,c100");
                load_test_config(rc1);

                REQUIRE(config.dump() == unindent(R"(
                                    default_channels:
                                      - c91
                                      - c100
                                      - c11
                                      - c12)"));

                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                std::string src1 = shrink_source(0);

                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
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
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
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
                REQUIRE(
                    ctx.default_channels
                    == config.at("default_channels").value<std::vector<std::string>>()
                );

                util::unset_env("MAMBA_DEFAULT_CHANNELS");
            }

            TEST_CASE_METHOD(Configuration, "channel_alias")
            {
                std::string rc1 = "channel_alias: http://repo.mamba.pm/";
                std::string rc2 = "channel_alias: https://conda.anaconda.org/";

                load_test_config({ rc1, rc2 });
                REQUIRE(config.dump() == "channel_alias: http://repo.mamba.pm/");

                load_test_config({ rc2, rc1 });
                REQUIRE(config.dump() == "channel_alias: https://conda.anaconda.org/");

                util::set_env("MAMBA_CHANNEL_ALIAS", "https://foo.bar");
                load_test_config(rc1);

                REQUIRE(config.dump() == "channel_alias: https://foo.bar");

                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                std::string src1 = shrink_source(0);

                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == "channel_alias: https://foo.bar  # 'MAMBA_CHANNEL_ALIAS' > '" + src1 + "'"
                );

                config.at("channel_alias").set_yaml_value("https://my.channel").compute();
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == "channel_alias: https://my.channel  # 'API' > 'MAMBA_CHANNEL_ALIAS' > '"
                           + src1 + "'"
                );
                REQUIRE(ctx.channel_alias == config.at("channel_alias").value<std::string>());

                util::unset_env("MAMBA_CHANNEL_ALIAS");
            }

            TEST_CASE_METHOD(Configuration, "mirrored_channels")
            {
                std::string rc1 = unindent(R"(
                    mirrored_channels:
                      conda-forge: [https://conda.anaconda.org/conda-forge, https://repo.mamba.pm/conda-forge]
                      channel1: [https://conda.anaconda.org/channel1]
                )");

                load_test_config(rc1);

                REQUIRE(config.dump() == unindent(R"(
                          mirrored_channels:
                            channel1:
                              - https://conda.anaconda.org/channel1
                            conda-forge:
                              - https://conda.anaconda.org/conda-forge
                              - https://repo.mamba.pm/conda-forge)"));
            }

            TEST_CASE_METHOD(Configuration, "envs_dirs")
            {
                // Load default config
                config.load();

                // `envs_dirs` should be set to `root_prefix / envs`
                const auto& envs_dirs = config.at("envs_dirs").value<std::vector<fs::u8path>>();

                REQUIRE(envs_dirs.size() == 1);
                REQUIRE(envs_dirs[0] == get_root_prefix_envs_dir());
            }

            TEST_CASE_METHOD(Configuration, "envs_dirs_with_additional_rc")
            {
                std::string cache1 = util::path_concat(util::user_home_dir(), "foo_envs_dirs");
                std::string rc1 = "envs_dirs:\n  - " + cache1;

                load_test_config(rc1);

                // `envs_dirs` should be set to the configured value `cache1`
                // and `root_prefix / envs`
                REQUIRE(
                    config.dump()
                    == "envs_dirs:\n  - " + cache1 + "\n  - " + get_root_prefix_envs_dir()
                );
            }

            TEST_CASE_METHOD(Configuration, "envs_dirs_with_env_variable")
            {
                std::string cache1 = util::path_concat(util::user_home_dir(), "foo_envs_dirs");
                std::string cache2 = util::path_concat(util::user_home_dir(), "bar_envs_dirs");

                // Set CONDA_ENVS_PATH with cache1 and cache2 using platform specific path separator
                util::set_env("CONDA_ENVS_PATH", cache1 + util::pathsep() + cache2);

                // Load default config to get the envs_dirs
                config.load();

                const auto& envs_dirs = config.at("envs_dirs").value<std::vector<fs::u8path>>();

                std::set<fs::u8path> envs_dirs_set(envs_dirs.begin(), envs_dirs.end());

                // `envs_dirs` should at least contain `root_prefix / envs`, `cache1` and `cache2`
                REQUIRE(envs_dirs.size() >= 3);
                REQUIRE(envs_dirs_set.find(get_root_prefix_envs_dir()) != envs_dirs_set.end());
                REQUIRE(envs_dirs_set.find(cache1) != envs_dirs_set.end());
                REQUIRE(envs_dirs_set.find(cache2) != envs_dirs_set.end());

                util::unset_env("CONDA_ENVS_PATH");
            }

            TEST_CASE_METHOD(Configuration, "pkgs_dirs")
            {
                std::string cache1 = util::path_concat(util::user_home_dir(), "foo");
                std::string cache2 = util::path_concat(util::user_home_dir(), "bar");

                std::string rc1 = "pkgs_dirs:\n  - " + cache1;
                std::string rc2 = "pkgs_dirs:\n  - " + cache2;

                load_test_config({ rc1, rc2 });
                REQUIRE(config.dump() == "pkgs_dirs:\n  - " + cache1 + "\n  - " + cache2);

                load_test_config({ rc2, rc1 });
                REQUIRE(config.dump() == "pkgs_dirs:\n  - " + cache2 + "\n  - " + cache1);

                std::string cache3 = util::path_concat(util::user_home_dir(), "baz");
                util::set_env("CONDA_PKGS_DIRS", cache3);
                load_test_config(rc1);
                REQUIRE(config.dump() == "pkgs_dirs:\n  - " + cache3 + "\n  - " + cache1);

                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                std::string src1 = shrink_source(0);

                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
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
                REQUIRE(
                    config.dump(
                        MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS | MAMBA_SHOW_ALL_CONFIGS,
                        { "pkgs_dirs" }
                    )
                    == unindent((R"(
                                    pkgs_dirs:
                                      - )"
                                 + (fs::u8path(root_prefix_str) / "pkgs").string() + R"(  # 'fallback'
                                      - )"
                                 + (fs::u8path(util::user_home_dir()) / ".mamba" / "pkgs").string()
                                 + R"(  # 'fallback')" + extra_cache)
                                    .c_str())
                );
                REQUIRE(ctx.pkgs_dirs == config.at("pkgs_dirs").value<std::vector<fs::u8path>>());

                std::string cache4 = util::path_concat(util::user_home_dir(), "babaz");
                util::set_env("CONDA_PKGS_DIRS", cache4);
                load_test_config(empty_rc);
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
                                    pkgs_dirs:
                                      - )"
                                 + cache4 + "  # 'CONDA_PKGS_DIRS'")
                                    .c_str())
                );

                util::unset_env("CONDA_PKGS_DIRS");
                util::unset_env("MAMBA_ROOT_PREFIX");
                config.clear_values();
            }

            TEST_CASE_METHOD(Configuration, "ssl_verify")
            {
                // Default empty string value
                ctx.remote_fetch_params.ssl_verify = "";
                std::string rc = "";
                load_test_config(rc);
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "<system>");

                rc = "ssl_verify: true";
                load_test_config(rc);
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "<system>");

                rc = "ssl_verify: <true>";
                load_test_config(rc);
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "<system>");

                rc = "ssl_verify: 1";
                load_test_config(rc);
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "<system>");

                rc = "ssl_verify: 10";
                load_test_config(rc);
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "10");

                rc = "ssl_verify: false";
                load_test_config(rc);
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "<false>");

                rc = "ssl_verify: <false>";
                load_test_config(rc);
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "<false>");

                rc = "ssl_verify: 0";
                load_test_config(rc);
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "<false>");

                rc = "ssl_verify: /foo/bar/baz";
                load_test_config(rc);
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "/foo/bar/baz");

                std::string rc1 = "ssl_verify: true";
                std::string rc2 = "ssl_verify: false";
                load_test_config({ rc1, rc2 });
                REQUIRE(config.at("ssl_verify").value<std::string>() == "<system>");
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "<system>");

                load_test_config({ rc2, rc1 });
                REQUIRE(config.at("ssl_verify").value<std::string>() == "<false>");
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "<false>");

                util::set_env("MAMBA_SSL_VERIFY", "/env/bar/baz");
                load_test_config(rc1);

                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                std::string src1 = shrink_source(0);

                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == "ssl_verify: /env/bar/baz  # 'MAMBA_SSL_VERIFY' > '" + src1 + "'"
                );

                config.at("ssl_verify").set_yaml_value("/new/test").compute();
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == "ssl_verify: /new/test  # 'API' > 'MAMBA_SSL_VERIFY' > '" + src1 + "'"
                );

                util::unset_env("MAMBA_SSL_VERIFY");
            }

#undef EXPECT_CA_EQUAL

            TEST_CASE_METHOD(Configuration, "cacert_path")
            {
                std::string rc = "ssl_verify: /foo/bar/baz\ncacert_path: /other/foo/bar/baz";
                load_test_config(rc);
                REQUIRE(config.at("ssl_verify").value<std::string>() == "/other/foo/bar/baz");
                REQUIRE(config.at("cacert_path").value<std::string>() == "/other/foo/bar/baz");
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "/other/foo/bar/baz");

                util::set_env("MAMBA_CACERT_PATH", "/env/ca/baz");
                load_test_config(rc);

                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                std::string src = shrink_source(0);

                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
                                    cacert_path: /env/ca/baz  # 'MAMBA_CACERT_PATH' > ')"
                                 + src + R"('
                                    ssl_verify: /env/ca/baz  # ')"
                                 + src + "'")
                                    .c_str())
                );
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "/env/ca/baz");

                config.at("cacert_path").set_yaml_value("/new/test").compute();
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
                                    cacert_path: /new/test  # 'API' > 'MAMBA_CACERT_PATH' > ')"
                                 + src + R"('
                                    ssl_verify: /env/ca/baz  # ')"
                                 + src + "'")
                                    .c_str())
                );
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "/env/ca/baz");

                config.at("ssl_verify").compute();
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
                                    cacert_path: /new/test  # 'API' > 'MAMBA_CACERT_PATH' > ')"
                                 + src + R"('
                                    ssl_verify: /new/test  # ')"
                                 + src + "'")
                                    .c_str())
                );
                REQUIRE(ctx.remote_fetch_params.ssl_verify == "/new/test");

                util::unset_env("MAMBA_CACERT_PATH");
                load_test_config("cacert_path:\nssl_verify: true");  // reset ssl verify to default
            }

            TEST_CASE_METHOD(Configuration, "proxy_servers")
            {
                std::string rc = unindent(R"(
                    proxy_servers:
                        http: foo
                        https: bar)");
                load_test_config(rc);
                auto& actual = config.at("proxy_servers").value<std::map<std::string, std::string>>();
                std::map<std::string, std::string> expected = { { "http", "foo" },
                                                                { "https", "bar" } };
                REQUIRE(actual == expected);
                REQUIRE(ctx.remote_fetch_params.proxy_servers == expected);

                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                REQUIRE(config.dump() == "proxy_servers:\n  http: foo\n  https: bar");
            }

            TEST_CASE_METHOD(Configuration, "platform")
            {
                REQUIRE(ctx.platform == ctx.host_platform);

                std::string rc = "platform: mylinux-128";
                load_test_config(rc);
                std::string src = shrink_source(0);
                REQUIRE(config.at("platform").value<std::string>() == "mylinux-128");
                REQUIRE(ctx.platform == "mylinux-128");
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
                                    platform: mylinux-128  # ')"
                                 + src + "'")
                                    .c_str())
                );

                util::set_env("CONDA_SUBDIR", "win-32");
                load_test_config(rc);
                src = shrink_source(0);
                REQUIRE(config.at("platform").value<std::string>() == "win-32");
                REQUIRE(ctx.platform == "win-32");
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
                                    platform: win-32  # 'CONDA_SUBDIR' > ')"
                                 + src + "'")
                                    .c_str())
                );

                config.at("platform").clear_values();
                ctx.platform = ctx.host_platform;
            }

#define TEST_BOOL_CONFIGURABLE(NAME, CTX)                                                           \
    TEST_CASE_METHOD(Configuration, #NAME)                                                          \
    {                                                                                               \
        std::string rc1 = std::string(#NAME) + ": true";                                            \
        std::string rc2 = std::string(#NAME) + ": false";                                           \
        if (config.at(#NAME).rc_configurable())                                                     \
        {                                                                                           \
            load_test_config({ rc1, rc2 });                                                         \
            REQUIRE(config.at(#NAME).value<bool>());                                                \
            REQUIRE(CTX);                                                                           \
                                                                                                    \
            load_test_config({ rc2, rc1 });                                                         \
            REQUIRE_FALSE(config.at(#NAME).value<bool>());                                          \
            REQUIRE_FALSE(CTX);                                                                     \
        }                                                                                           \
                                                                                                    \
        std::string env_name = "MAMBA_" + util::to_upper(#NAME);                                    \
        util::set_env(env_name, "true");                                                            \
        load_test_config(rc2);                                                                      \
                                                                                                    \
        REQUIRE(config.sources().size() == 1);                                                      \
        REQUIRE(config.valid_sources().size() == 1);                                                \
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
        REQUIRE((config.dump(dump_opts, { #NAME })) == expected);                                   \
        REQUIRE(config.at(#NAME).value<bool>());                                                    \
        REQUIRE(CTX);                                                                               \
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
        REQUIRE((config.dump(dump_opts, { #NAME })) == expected);                                   \
        REQUIRE(config.at(#NAME).value<bool>());                                                    \
        REQUIRE(CTX);                                                                               \
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

            TEST_CASE_METHOD(Configuration, "channel_priority")
            {
                std::string rc1 = "channel_priority: flexible";
                std::string rc2 = "channel_priority: strict";
                std::string rc3 = "channel_priority: disabled";

                load_test_config({ rc1, rc2, rc3 });
                REQUIRE(
                    config.at("channel_priority").value<ChannelPriority>() == ChannelPriority::Flexible
                );
                REQUIRE(ctx.channel_priority == ChannelPriority::Flexible);

                load_test_config({ rc3, rc1, rc2 });
                REQUIRE(
                    config.at("channel_priority").value<ChannelPriority>() == ChannelPriority::Disabled
                );
                REQUIRE(ctx.channel_priority == ChannelPriority::Disabled);

                load_test_config({ rc2, rc1, rc3 });
                REQUIRE(
                    config.at("channel_priority").value<ChannelPriority>() == ChannelPriority::Strict
                );
                REQUIRE(ctx.channel_priority == ChannelPriority::Strict);

                util::set_env("MAMBA_CHANNEL_PRIORITY", "strict");
                load_test_config(rc3);

                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                std::string src = shrink_source(0);

                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == "channel_priority: strict  # 'MAMBA_CHANNEL_PRIORITY' > '" + src + "'"
                );
                REQUIRE(
                    config.at("channel_priority").value<ChannelPriority>() == ChannelPriority::Strict
                );
                REQUIRE(ctx.channel_priority == ChannelPriority::Strict);

                config.at("channel_priority").set_yaml_value("flexible").compute();
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == "channel_priority: flexible  # 'API' > 'MAMBA_CHANNEL_PRIORITY' > '" + src + "'"
                );
                REQUIRE(
                    config.at("channel_priority").value<ChannelPriority>() == ChannelPriority::Flexible
                );
                REQUIRE(ctx.channel_priority == ChannelPriority::Flexible);

                util::set_env("MAMBA_CHANNEL_PRIORITY", "stric");
                REQUIRE_THROWS_AS(load_test_config(rc3), YAML::Exception);

                util::unset_env("MAMBA_CHANNEL_PRIORITY");
            }

            TEST_CASE_METHOD(Configuration, "skip_misformatted_config_file")
            {
                std::string rc = "invalid_scalar_value";
                load_test_config(rc);
                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 0);
                REQUIRE(config.dump() == "");
            }

            TEST_CASE_METHOD(Configuration, "pinned_packages")
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
                REQUIRE(config.dump() == unindent(R"(
                                            pinned_packages:
                                              - jupyterlab=3
                                              - numpy=1.19
                                              - matplotlib
                                              - bokeh)"));
                REQUIRE(
                    ctx.pinned_packages
                    == std::vector<std::string>({ "jupyterlab=3", "numpy=1.19", "matplotlib", "bokeh" })
                );

                load_test_config({ rc2, rc1, rc3 });
                REQUIRE(config.at("pinned_packages").yaml_value());
                REQUIRE(config.dump() == unindent(R"(
                                            pinned_packages:
                                              - matplotlib
                                              - numpy=1.19
                                              - jupyterlab=3
                                              - bokeh)"));
                REQUIRE(
                    ctx.pinned_packages
                    == std::vector<std::string>({ "matplotlib", "numpy=1.19", "jupyterlab=3", "bokeh" })
                );

                util::set_env("MAMBA_PINNED_PACKAGES", "mpl=10.2,xtensor");
                load_test_config(rc1);
                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                std::string src1 = shrink_source(0);

                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
                                    pinned_packages:
                                      - mpl=10.2  # 'MAMBA_PINNED_PACKAGES'
                                      - xtensor  # 'MAMBA_PINNED_PACKAGES'
                                      - jupyterlab=3  # ')"
                                 + src1 + R"('
                                      - numpy=1.19  # ')"
                                 + src1 + "'")
                                    .c_str())
                );
                REQUIRE(
                    ctx.pinned_packages
                    == std::vector<std::string>({ "mpl=10.2", "xtensor", "jupyterlab=3", "numpy=1.19" })
                );

                config.at("pinned_packages").set_yaml_value("pytest").compute();
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == unindent((R"(
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
                REQUIRE(
                    ctx.pinned_packages
                    == std::vector<std::string>(
                        { "pytest", "mpl=10.2", "xtensor", "jupyterlab=3", "numpy=1.19" }
                    )
                );

                util::unset_env("MAMBA_PINNED_PACKAGES");
            }

            TEST_BOOL_CONFIGURABLE(no_pin, config.at("no_pin").value<bool>());

            TEST_BOOL_CONFIGURABLE(retry_clean_cache, config.at("retry_clean_cache").value<bool>());

            TEST_BOOL_CONFIGURABLE(allow_softlinks, ctx.link_params.allow_softlinks);

            TEST_BOOL_CONFIGURABLE(always_softlink, ctx.link_params.always_softlink);

            TEST_BOOL_CONFIGURABLE(always_copy, ctx.link_params.always_copy);

            TEST_CASE_METHOD(Configuration, "always_softlink_and_copy")
            {
                util::set_env("MAMBA_ALWAYS_COPY", "true");
                REQUIRE_THROWS_AS(load_test_config("always_softlink: true"), std::runtime_error);
                util::unset_env("MAMBA_ALWAYS_COPY");

                util::set_env("MAMBA_ALWAYS_SOFTLINK", "true");
                REQUIRE_THROWS_AS(load_test_config("always_copy: true"), std::runtime_error);
                util::unset_env("MAMBA_ALWAYS_SOFTLINK");

                load_test_config("always_softlink: false\nalways_copy: false");
            }

            TEST_CASE_METHOD(Configuration, "safety_checks")
            {
                std::string rc1 = "safety_checks: enabled";
                std::string rc2 = "safety_checks: warn";
                std::string rc3 = "safety_checks: disabled";

                load_test_config({ rc1, rc2, rc3 });
                REQUIRE(
                    config.at("safety_checks").value<VerificationLevel>() == VerificationLevel::Enabled
                );
                REQUIRE(ctx.validation_params.safety_checks == VerificationLevel::Enabled);

                load_test_config({ rc2, rc1, rc3 });
                REQUIRE(
                    config.at("safety_checks").value<VerificationLevel>() == VerificationLevel::Warn
                );
                REQUIRE(ctx.validation_params.safety_checks == VerificationLevel::Warn);

                load_test_config({ rc3, rc1, rc3 });
                REQUIRE(
                    config.at("safety_checks").value<VerificationLevel>() == VerificationLevel::Disabled
                );
                REQUIRE(ctx.validation_params.safety_checks == VerificationLevel::Disabled);

                util::set_env("MAMBA_SAFETY_CHECKS", "warn");
                load_test_config(rc1);

                REQUIRE(config.sources().size() == 1);
                REQUIRE(config.valid_sources().size() == 1);
                std::string src = shrink_source(0);

                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == "safety_checks: warn  # 'MAMBA_SAFETY_CHECKS' > '" + src + "'"
                );
                REQUIRE(
                    config.at("safety_checks").value<VerificationLevel>() == VerificationLevel::Warn
                );
                REQUIRE(ctx.validation_params.safety_checks == VerificationLevel::Warn);

                config.at("safety_checks").set_yaml_value("disabled").compute();
                REQUIRE(
                    config.dump(MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS)
                    == "safety_checks: disabled  # 'API' > 'MAMBA_SAFETY_CHECKS' > '" + src + "'"
                );
                REQUIRE(
                    config.at("safety_checks").value<VerificationLevel>() == VerificationLevel::Disabled
                );
                REQUIRE(ctx.validation_params.safety_checks == VerificationLevel::Disabled);

                util::set_env("MAMBA_SAFETY_CHECKS", "yeap");
                REQUIRE_THROWS_AS(load_test_config(rc2), std::runtime_error);

                util::unset_env("MAMBA_SAFETY_CHECKS");
                load_test_config(rc2);
            }

            TEST_BOOL_CONFIGURABLE(extra_safety_checks, ctx.validation_params.extra_safety_checks);

#undef TEST_BOOL_CONFIGURABLE

            TEST_CASE_METHOD(Configuration, "has_config_name")
            {
                using namespace detail;

                REQUIRE_FALSE(has_config_name(""));
                REQUIRE_FALSE(has_config_name("conf"));
                REQUIRE_FALSE(has_config_name("config"));
                REQUIRE_FALSE(has_config_name("config.conda"));
                REQUIRE_FALSE(has_config_name("conf.condarc"));
                REQUIRE_FALSE(has_config_name("conf.mambarc"));

                REQUIRE(has_config_name("condarc"));
                REQUIRE(has_config_name("mambarc"));
                REQUIRE(has_config_name(".condarc"));
                REQUIRE(has_config_name(".mambarc"));
                REQUIRE(has_config_name(".yaml"));
                REQUIRE(has_config_name(".yml"));
                REQUIRE(has_config_name("conf.yaml"));
                REQUIRE(has_config_name("config.yml"));
            }

            TEST_CASE_METHOD(Configuration, "is_config_file")
            {
                using namespace detail;

                fs::u8path p = mambatests::test_data_dir / "config/.condarc";

                std::vector<fs::u8path> wrong_paths = {
                    mambatests::test_data_dir / "config",
                    mambatests::test_data_dir / "conf",
                    mambatests::test_data_dir / "config/condarc",
                    mambatests::test_data_dir / "history/conda-meta/history",
                };

                REQUIRE(is_config_file(p));

                for (const fs::u8path& wp : wrong_paths)
                {
                    REQUIRE_FALSE(is_config_file(wp));
                }
            }

            // Regression test for https://github.com/mamba-org/mamba/issues/2704
            TEST_CASE_METHOD(Configuration, "deduplicate_rc_files")
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

                REQUIRE(config.sources().size() == 2);
                REQUIRE(config.at("channel_alias").value<std::string>() == "http://inner.com");
            }

            TEST_CASE_METHOD(Configuration, "print_scalar_node")
            {
                using namespace detail;

                std::string rc = "foo";
                auto node = YAML::Load(rc);
                auto node_src = YAML::Load("/some/source1");
                YAML::Emitter out;
                print_scalar_node(out, node, node_src, true);

                std::string res = out.c_str();
                REQUIRE(res == "foo  # '/some/source1'");

                // These tests do not really make sense since
                // print_scalar should be called by print_configurable only
                // and the check is already done in it.
                /*
                rc = unindent(R"(
                                foo: bar
                                bar: baz)");
                node = YAML::Load(rc);
                REQUIRE_THROWS_AS(print_scalar_node(out, node, node_src, true), std::runtime_error);

                rc = unindent(R"(
                                - foo
                                - bar)");
                node = YAML::Load(rc);
                REQUIRE_THROWS_AS(print_scalar_node(out, node, node_src, true), std::runtime_error);

                node = YAML::Node();
                REQUIRE_THROWS_AS(print_scalar_node(out, node, node_src, true), std::runtime_error);
                */
            }

            TEST_CASE_METHOD(Configuration, "print_map_node")
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
                REQUIRE(res == unindent(R"(
                                    foo: bar  # '/some/source1'
                                    bar: baz  # '/some/source2')"));

                // These tests do not really make sense since
                // print_scalar should be called by print_configurable only
                // and the check is already done in it.
                /*rc = "foo";
                node = YAML::Load(rc);
                REQUIRE_THROWS_AS(print_map_node(out, node, node_src, true), std::runtime_error);

                rc = unindent(R"(
                                - foo
                                - bar)");
                node = YAML::Load(rc);
                REQUIRE_THROWS_AS(print_map_node(out, node, node_src, true), std::runtime_error);

                node = YAML::Node();
                REQUIRE_THROWS_AS(print_map_node(out, node, node_src, true), std::runtime_error);*/
            }

            TEST_CASE_METHOD(Configuration, "print_seq_node")
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
                REQUIRE(res == unindent(R"(
                                      - foo  # '/some/source1'
                                      - bar  # '/some/source2')"));

                // These tests do not really make sense since
                // print_scalar should be called by print_configurable only
                // and the check is already done in it.
                /*rc = "foo";
                node = YAML::Load(rc);
                REQUIRE_THROWS_AS(print_seq_node(out, node, node_src, true), std::runtime_error);

                rc = unindent(R"(
                                foo: bar
                                bar: baz)");
                node = YAML::Load(rc);
                REQUIRE_THROWS_AS(print_seq_node(out, node, node_src, true), std::runtime_error);

                node = YAML::Node();
                REQUIRE_THROWS_AS(print_seq_node(out, node, node_src, true), std::runtime_error);*/
            }
        }
    }  // namespace testing
}  // namespace mamba
