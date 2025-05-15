// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <catch2/catch_all.hpp>

#include "mambatests.hpp"

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "mamba/core/channel_context.hpp"
#include "mamba/core/history.hpp"
#include "mamba/core/prefix_data.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("History parse")
        {
            static const auto history_file_path = fs::absolute(
                mambatests::test_data_dir / "history/parse/conda-meta/history"
            );
            static const auto aux_file_path = fs::absolute(
                mambatests::test_data_dir / "history/parse/conda-meta/aux_file"
            );

            // Backup history file and restore it at the end of the test, whatever the output.
            struct ScopedHistoryFileBackup
            {
                ScopedHistoryFileBackup()
                {
                    fs::remove(aux_file_path);
                    fs::copy(history_file_path, aux_file_path);
                }

                ~ScopedHistoryFileBackup()
                {
                    fs::remove(history_file_path);
                    fs::copy(aux_file_path, history_file_path);
                }
            } scoped_history_file_backup;

            auto channel_context = ChannelContext::make_conda_compatible(mambatests::context());

            // Gather history from current history file.
            History history_instance(mambatests::test_data_dir / "history/parse", channel_context);
            std::vector<History::UserRequest> user_reqs = history_instance.get_user_requests();

            // Extract raw history file content into buffer.
            std::ifstream history_file(history_file_path.std_path());
            std::stringstream original_history_buffer;
            std::string line;
            while (getline(history_file, line))
            {
                original_history_buffer << line;
            }
            history_file.close();

            // Generate a history buffer with duplicate history.
            std::stringstream check_buffer;
            check_buffer << original_history_buffer.str() << original_history_buffer.str();

            std::cout << check_buffer.str() << '\n';

            // Re-inject history into history file: history file should then have the same duplicate
            // content as the buffer.
            for (const auto& req : user_reqs)
            {
                history_instance.add_entry(req);
            }

            history_file.open(history_file_path.std_path());
            std::stringstream updated_history_buffer;
            while (getline(history_file, line))
            {
                updated_history_buffer << line;
            }
            history_file.close();

            REQUIRE(updated_history_buffer.str() == check_buffer.str());
        }

        TEST_CASE("parse_metadata")
        {
            auto channel_context = ChannelContext::make_conda_compatible(mambatests::context());

            History history_instance(
                mambatests::test_data_dir / "history/parse_metadata",
                channel_context
            );
            // Must not throw
            std::vector<History::UserRequest> user_reqs = history_instance.get_user_requests();
        }

        TEST_CASE("parse_all_formats")
        {
            std::vector<std::string> test_list{
                "conda-forge/linux-64::xtl-0.8.0-h84d6215_0",
                "conda-forge::xtl-0.8.0-h84d6215_0",
                "https://conda.anaconda.org/conda-forge/linux-64::xtl-0.8.0-h84d6215_0"
            };
            for (auto s : test_list)
            {
                specs::PackageInfo pkg_info = mamba::detail::pkg_info_builder(s);
                REQUIRE(pkg_info.name == "xtl");
                REQUIRE(pkg_info.version == "0.8.0");
                REQUIRE(pkg_info.channel == "conda-forge");
                REQUIRE(pkg_info.build_string == "h84d6215_0");
            }
        }

        TEST_CASE("revision_diff")
        {
            auto channel_context = ChannelContext::make_conda_compatible(mambatests::context());

            // Gather history from current history file.
            History history_instance(mambatests::test_data_dir / "history/parse", channel_context);
            std::vector<History::UserRequest> user_requests = history_instance.get_user_requests();
            std::size_t target_revision = 1;

            detail::PackageDiff pkg_diff{};
            pkg_diff = pkg_diff.from_revision(user_requests, target_revision);
            const auto& removed_pkg_diff = pkg_diff.removed_pkg_diff;
            const auto& installed_pkg_diff = pkg_diff.installed_pkg_diff;

            REQUIRE(removed_pkg_diff.find("nlohmann_json")->second.version == "3.12.0");
            REQUIRE(removed_pkg_diff.find("xtl")->second.version == "0.7.2");
            REQUIRE(installed_pkg_diff.find("cpp-tabulate")->second.version == "1.5");
            REQUIRE(installed_pkg_diff.find("wheel")->second.version == "0.40.0");
            REQUIRE(installed_pkg_diff.find("openssl")->second.version == "3.5.0");
            REQUIRE(installed_pkg_diff.find("xtl")->second.version == "0.8.0");
        }

#ifndef _WIN32
        TEST_CASE("parse_segfault")
        {
            pid_t child = fork();
            auto channel_context = ChannelContext::make_conda_compatible(mambatests::context());
            if (child)
            {
                int wstatus;
                waitpid(child, &wstatus, 0);
                REQUIRE(WIFEXITED(wstatus));
            }
            else
            {
                History history_instance("history_test/parse_segfault", channel_context);
                history_instance.get_user_requests();
                exit(EXIT_SUCCESS);
            }
        }
#endif
    }
}  // namespace mamba
