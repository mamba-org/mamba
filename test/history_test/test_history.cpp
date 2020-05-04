#include <gtest/gtest.h>
#include <fstream>
#include <string>

#include "history.hpp"

namespace mamba
{
    TEST(history, parse)
    {
        History history_instance("../history_test/");

        std::vector<History::UserRequest> user_reqs = history_instance.get_user_requests();

        std::ifstream history_file("../history_test/conda-meta/history");
        std::stringstream original_history_buffer;
        std::string line;
        while (getline(history_file, line))
        {
            original_history_buffer << line;
        }
        // original_history_buffer << history_file.rdbuf();
        history_file.close();

        std::stringstream check_buffer;
        check_buffer << original_history_buffer.str() << original_history_buffer.str();
        // std::cout << check_buffer.str() << std::endl;
        // std::cout << "============================================" << std::endl;

        history_instance.add_entry(user_reqs);

        history_file.open("../history_test/conda-meta/history");
        std::stringstream updated_history_buffer;
        // updated_history_buffer << history_file.rdbuf();
        while (getline(history_file, line))
        {
            updated_history_buffer << line;
        }

        std::cout << updated_history_buffer.str() << std::endl;
        ASSERT_EQ(updated_history_buffer.str(), check_buffer.str());

        history_file.close();

        // std::ofstream clean_aux_file("aux_file", std::ios::out | std::ios::trunc);
        // aux_file.close();
        // clean_aux_file.close();
    }
}
