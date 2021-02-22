#include <gtest/gtest.h>

#include "mamba/config.hpp"
#include "mamba/context.hpp"

#include <cstdio>


namespace mamba
{
    namespace testing
    {
        class Configurable
            : public mamba::Configurable
            , public ::testing::Test
        {
        public:
            Configurable()
                : mamba::Configurable("")  // prevent loading other sources
            {
            }

        protected:
            void load_test_config(std::string rc)
            {
                std::string unique_location = tempfile_ptr->path();

                std::ofstream out_file(unique_location);
                out_file << rc;
                out_file.close();

                sources.clear();
                sources.push_back(unique_location);

                load_config();
            }

            void load_test_config(std::vector<std::string> rcs)
            {
                std::vector<std::unique_ptr<TemporaryFile>> tempfiles;

                sources.clear();
                for (auto rc : rcs)
                {
                    tempfiles.push_back(std::make_unique<TemporaryFile>("mambarc", ".yaml"));
                    std::string loc = tempfiles.back()->path();

                    std::ofstream out_file(loc);
                    out_file << rc;
                    out_file.close();

                    sources.push_back(loc);
                }

                load_config();
            }

            std::unique_ptr<TemporaryFile> tempfile_ptr
                = std::make_unique<TemporaryFile>("mambarc", ".yaml");
        };

        TEST_F(Configurable, load_rc_file)
        {
            std::string rc = unindent(R"(
                channels:
                    - test1)");

            load_test_config(rc);
            std::string src = tempfile_ptr->path();

            EXPECT_EQ(sources.size(), 1);
            EXPECT_EQ(valid_sources.size(), 1);
            EXPECT_EQ(dump(), "channels:\n  - test1");
            EXPECT_EQ(dump(true), "channels:\n  - test1  # " + src);

            // Hill-formed config file
            rc = unindent(R"(
                channels:
                    - test1
                   - https://repo.mamba.pm/conda-forge)");

            load_test_config(rc);

            EXPECT_EQ(sources.size(), 1);
            EXPECT_EQ(valid_sources.size(), 0);
            EXPECT_EQ(dump(), "");
            EXPECT_EQ(dump(true), "");
        }

        TEST_F(Configurable, load_config_files)
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

            ASSERT_EQ(sources.size(), 2);
            ASSERT_EQ(valid_sources.size(), 2);

            std::string src1 = valid_sources[0];
            std::string src2 = valid_sources[1];
            EXPECT_EQ(dump(), unindent(R"(
                                    channels:
                                      - test1
                                      - test2
                                    ssl_verify: false)"));
            EXPECT_EQ(dump(true),
                      unindent((R"(
                                    channels:
                                      - test1  # )"
                                + src1 + R"(
                                      - test2  # )"
                                + src2 + R"(
                                    ssl_verify: false  # )"
                                + src1)
                                   .c_str()));

            // hill-formed key
            std::string rc3 = unindent(R"(
                channels:
                    - test3
                override_channels_enabled:
                    - false)");
            rcs.push_back(rc3);
            load_test_config(rcs);

            ASSERT_EQ(sources.size(), 3);
            ASSERT_EQ(valid_sources.size(), 3);

            // tmp files changed
            src1 = valid_sources[0];
            src2 = valid_sources[1];
            std::string src3 = valid_sources[2];
            EXPECT_EQ(dump(), unindent(R"(
                                    channels:
                                      - test1
                                      - test2
                                      - test3
                                    ssl_verify: false)"));
            EXPECT_EQ(dump(true),
                      unindent((R"(
                                    channels:
                                      - test1  # )"
                                + src1 + R"(
                                      - test2  # )"
                                + src2 + R"(
                                      - test3  # )"
                                + src3 + R"(
                                    ssl_verify: false  # )"
                                + src1)
                                   .c_str()));

            // hill-formed file
            std::string rc4 = unindent(R"(
                channels:
                  - test3
                 - test4)");
            rcs.push_back(rc4);
            load_test_config(rcs);

            ASSERT_EQ(sources.size(), 4);
            ASSERT_EQ(valid_sources.size(), 3);

            // tmp files changed
            src1 = valid_sources[0];
            src2 = valid_sources[1];
            src3 = valid_sources[2];
            EXPECT_EQ(dump(), unindent(R"(
                                    channels:
                                      - test1
                                      - test2
                                      - test3
                                    ssl_verify: false)"));
            EXPECT_EQ(dump(true),
                      unindent((R"(
                                    channels:
                                      - test1  # )"
                                + src1 + R"(
                                      - test2  # )"
                                + src2 + R"(
                                      - test3  # )"
                                + src3 + R"(
                                    ssl_verify: false  # )"
                                + src1)
                                   .c_str()));
        }

        TEST_F(Configurable, has_config_extension)
        {
            EXPECT_FALSE(has_config_extension(""));
            EXPECT_FALSE(has_config_extension("condarc"));
            EXPECT_FALSE(has_config_extension("mambarc"));
            EXPECT_FALSE(has_config_extension("conf"));
            EXPECT_FALSE(has_config_extension("config"));
            EXPECT_FALSE(has_config_extension("config.conda"));
            EXPECT_FALSE(has_config_extension("conf.condarc"));
            EXPECT_FALSE(has_config_extension("conf.mambarc"));

            EXPECT_TRUE(has_config_extension(".condarc"));
            EXPECT_TRUE(has_config_extension(".mambarc"));
            EXPECT_TRUE(has_config_extension(".yaml"));
            EXPECT_TRUE(has_config_extension(".yml"));
            EXPECT_TRUE(has_config_extension("conf.yaml"));
            EXPECT_TRUE(has_config_extension("config.yml"));
        }

        TEST_F(Configurable, looks_config_file)
        {
            fs::path p = "config_test/.condarc";

            std::vector<fs::path> wrong_paths = {
                "config_test", "conf_test", "config_test/condarc", "history_test/conda-meta/history"
            };

            EXPECT_TRUE(looks_config_file(p));

            for (fs::path p : wrong_paths)
            {
                EXPECT_FALSE(looks_config_file(p));
            }
        }

        TEST_F(Configurable, print_scalar_with_sources)
        {
            std::string rc = "foo";
            auto node = YAML::Load(rc);
            auto node_src = YAML::Load("/some/source1");
            YAML::Emitter out;
            print_scalar_with_sources(out, node, node_src);

            std::string res = out.c_str();
            EXPECT_EQ(res, "foo  # /some/source1");

            rc = unindent(R"(
                            foo: bar
                            bar: baz)");
            node = YAML::Load(rc);
            EXPECT_THROW(print_scalar_with_sources(out, node, node_src), std::runtime_error);

            rc = unindent(R"(
                            - foo
                            - bar)");
            node = YAML::Load(rc);
            EXPECT_THROW(print_scalar_with_sources(out, node, node_src), std::runtime_error);

            node = YAML::Node();
            EXPECT_THROW(print_scalar_with_sources(out, node, node_src), std::runtime_error);
        }

        TEST_F(Configurable, print_map_with_sources)
        {
            std::string rc = unindent(R"(
                                foo: bar
                                bar: baz)");
            auto node = YAML::Load(rc);
            auto node_src = YAML::Load(unindent(R"(
                                          foo: /some/source1
                                          bar: /some/source2)"));
            YAML::Emitter out;
            print_map_with_sources(out, node, node_src);

            std::string res = out.c_str();
            EXPECT_EQ(res, unindent(R"(
                                foo: bar  # /some/source1
                                bar: baz  # /some/source2)"));

            rc = "foo";
            node = YAML::Load(rc);
            EXPECT_THROW(print_map_with_sources(out, node, node_src), std::runtime_error);

            rc = unindent(R"(
                            - foo
                            - bar)");
            node = YAML::Load(rc);
            EXPECT_THROW(print_map_with_sources(out, node, node_src), std::runtime_error);

            node = YAML::Node();
            EXPECT_THROW(print_map_with_sources(out, node, node_src), std::runtime_error);
        }

        TEST_F(Configurable, print_seq_with_sources)
        {
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
            print_seq_with_sources(out, node, node_src);

            std::string res = out.c_str();
            EXPECT_EQ(res, unindent(R"(
                                  - foo  # /some/source1
                                  - bar  # /some/source2)"));

            rc = "foo";
            node = YAML::Load(rc);
            EXPECT_THROW(print_seq_with_sources(out, node, node_src), std::runtime_error);

            rc = unindent(R"(
                            foo: bar
                            bar: baz)");
            node = YAML::Load(rc);
            EXPECT_THROW(print_seq_with_sources(out, node, node_src), std::runtime_error);

            node = YAML::Node();
            EXPECT_THROW(print_seq_with_sources(out, node, node_src), std::runtime_error);
        }

        TEST_F(Configurable, print)
        {
            std::string rc = unindent(R"(
                channels:
                    - test1
                    - https://repo.mamba.pm/conda-forge
                override_channels_enabled: true
                ssl_verify: true
                test_complex_structure:
                    - foo: bar
                    - bar: baz)");

            load_test_config(rc);

            std::string src = tempfile_ptr->path();
            ASSERT_EQ(sources.size(), 1);
            EXPECT_EQ(sources[0], src);

            std::string res = dump();
            // Unexpected/handled keys are dropped
            EXPECT_EQ(res, unindent(R"(
                                channels:
                                  - test1
                                  - https://repo.mamba.pm/conda-forge
                                ssl_verify: true
                                override_channels_enabled: true)"));

            res = dump(true);
            EXPECT_EQ(res,
                      unindent((R"(
                                channels:
                                  - test1  # )"
                                + src + R"(
                                  - https://repo.mamba.pm/conda-forge  # )"
                                + src + R"(
                                ssl_verify: true  # )"
                                + src + R"(
                                override_channels_enabled: true  # )"
                                + src)
                                   .c_str()));
        }

        TEST_F(Configurable, channels)
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
            load_test_config(std::vector<std::string>({ rc1, rc2, rc3 }));

            ASSERT_TRUE(config["channels"]);
            EXPECT_EQ(dump(), unindent(R"(
                        channels:
                          - c11
                          - c12
                          - c21
                          - c32)"));
        }

        TEST_F(Configurable, default_channels)
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
            load_test_config(std::vector<std::string>({ rc1, rc2, rc3 }));

            ASSERT_TRUE(config["default_channels"]);
            EXPECT_EQ(dump(), unindent(R"(
                        default_channels:
                          - c11
                          - c12
                          - c21
                          - c32)"));
        }

        TEST_F(Configurable, channel_alias)
        {
            std::string rc1 = "channel_alias: http://repo.mamba.pm/";
            std::string rc2 = "channel_alias: https://conda.anaconda.org/";

            load_test_config(std::vector<std::string>({ rc1, rc2 }));
            ASSERT_TRUE(config["channel_alias"]);
            EXPECT_EQ(config["channel_alias"].as<std::string>(), "http://repo.mamba.pm/");

            load_test_config(std::vector<std::string>({ rc2, rc1 }));
            ASSERT_TRUE(config["channel_alias"]);
            EXPECT_EQ(config["channel_alias"].as<std::string>(), "https://conda.anaconda.org/");
        }

        TEST_F(Configurable, ssl_verify)
        {
            std::string rc1 = "ssl_verify: true";
            std::string rc2 = "ssl_verify: false";

            load_test_config(std::vector<std::string>({ rc1, rc2 }));
            ASSERT_TRUE(config["ssl_verify"]);
            ASSERT_TRUE(config["ssl_verify"].as<bool>());

            load_test_config(std::vector<std::string>({ rc2, rc1 }));
            ASSERT_TRUE(config["ssl_verify"]);
            ASSERT_FALSE(config["ssl_verify"].as<bool>());
        }

        TEST_F(Configurable, override_channels_enabled)
        {
            std::string rc1 = "override_channels_enabled: true";
            std::string rc2 = "override_channels_enabled: false";

            load_test_config(std::vector<std::string>({ rc1, rc2 }));
            ASSERT_TRUE(config["override_channels_enabled"]);
            ASSERT_TRUE(config["override_channels_enabled"].as<bool>());

            load_test_config(std::vector<std::string>({ rc2, rc1 }));
            ASSERT_TRUE(config["override_channels_enabled"]);
            ASSERT_FALSE(config["override_channels_enabled"].as<bool>());
        }

        TEST_F(Configurable, auto_activate_base)
        {
            std::string rc1 = "auto_activate_base: true";
            std::string rc2 = "auto_activate_base: false";

            load_test_config(std::vector<std::string>({ rc1, rc2 }));
            ASSERT_TRUE(config["auto_activate_base"]);
            ASSERT_TRUE(config["auto_activate_base"].as<bool>());

            load_test_config(std::vector<std::string>({ rc2, rc1 }));
            ASSERT_TRUE(config["auto_activate_base"]);
            ASSERT_FALSE(config["auto_activate_base"].as<bool>());
        }

        TEST_F(Configurable, channel_priority)
        {
            std::string rc1 = "channel_priority: flexible";
            std::string rc2 = "channel_priority: strict";
            std::string rc3 = "channel_priority: disabled";

            load_test_config(std::vector<std::string>({ rc1, rc2, rc3 }));
            ASSERT_TRUE(config["channel_priority"]);
            ASSERT_EQ(config["channel_priority"].as<std::string>(), "flexible");

            load_test_config(std::vector<std::string>({ rc3, rc1, rc2 }));
            ASSERT_TRUE(config["channel_priority"]);
            ASSERT_EQ(config["channel_priority"].as<std::string>(), "disabled");

            load_test_config(std::vector<std::string>({ rc2, rc1, rc3 }));
            ASSERT_TRUE(config["channel_priority"]);
            ASSERT_EQ(config["channel_priority"].as<std::string>(), "strict");
        }
    }  // namespace testing
}  // namespace mamba
