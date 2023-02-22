#include <string>

#include <gtest/gtest.h>

#include "mamba/core/history.hpp"

#include "test_data.hpp"

namespace mamba
{
    TEST(history, parse)
    {
        static const auto history_file_path = fs::absolute(
            test_data_dir / "history_test/parse/conda-meta/history"
        );
        static const auto aux_file_path = fs::absolute(
            test_data_dir / "history_test/parse/conda-meta/aux_file"
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

        // Gather history from current history file.
        History history_instance(test_data_dir / "history_test/parse");
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

        ASSERT_EQ(updated_history_buffer.str(), check_buffer.str());
    }

#ifndef _WIN32
    TEST(history, parse_segfault)
    {
        pid_t child = fork();
        if (child)
        {
            int wstatus;
            waitpid(child, &wstatus, 0);
            ASSERT_TRUE(WIFEXITED(wstatus));
        }
        else
        {
            History history_instance("history_test/parse_segfault");
            history_instance.get_user_requests();
            exit(EXIT_SUCCESS);
        }
    }
#endif
}  // namespace mamba
