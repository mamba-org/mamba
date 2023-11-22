// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <doctest/doctest.h>

#include "mambatests.hpp"

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "mamba/core/channel_context.hpp"
#include "mamba/core/history.hpp"

#include "mambatests.hpp"

namespace mamba
{
    TEST_SUITE("history")
    {
        TEST_CASE("parse")
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

            REQUIRE_EQ(updated_history_buffer.str(), check_buffer.str());
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
