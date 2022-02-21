#include <gtest/gtest.h>

#include <fstream>
#include <string>

#include "mamba/core/history.hpp"

namespace mamba
{
    TEST(history, parse)
    {
        static constexpr auto history_file_path = "history_test/parse/conda-meta/history";
        static constexpr auto aux_file_path = "history_test/parse/conda-meta/aux_file";

        History history_instance("history_test/parse");

        std::vector<History::UserRequest> user_reqs = history_instance.get_user_requests();

        std::ifstream history_file(history_file_path);
        std::stringstream original_history_buffer;
        std::string line;
        while (getline(history_file, line))
        {
            original_history_buffer << line;
        }
        history_file.close();

        fs::remove(aux_file_path);
        fs::copy(history_file_path, aux_file_path);

        std::stringstream check_buffer;
        check_buffer << original_history_buffer.str() << original_history_buffer.str();

        std::cout << check_buffer.str() << std::endl;
        for (const auto& req : user_reqs)
        {
            history_instance.add_entry(req);
        }

        history_file.open(history_file_path);
        std::stringstream updated_history_buffer;
        while (getline(history_file, line))
        {
            updated_history_buffer << line;
        }
        history_file.close();

        ASSERT_EQ(updated_history_buffer.str(), check_buffer.str());

        fs::remove(history_file_path);
        fs::copy(aux_file_path, history_file_path);
    }

#ifndef _WIN32
    TEST(history, parse_segfault)
    {
        pid_t child = fork();
        if (child) {
            int wstatus;
            waitpid(child, &wstatus, 0);
            ASSERT_TRUE(WIFEXITED(wstatus));
        } else {
            History history_instance("history_test/parse_segfault");
            history_instance.get_user_requests();
            exit(0);
        }
    }
#endif
}  // namespace mamba
