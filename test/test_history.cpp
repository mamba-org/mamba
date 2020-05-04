#include <gtest/gtest.h>
#include <fstream>

#include "history.hpp"

namespace mamba
{
    TEST(history, parse)
    {
        History original_history("original_history_data");
        History aux_history("aux_history_data");

        std::vector<History::UserRequest> user_reqs = original_history.get_user_requests();

        std::ifstream original_history_file("original_history_data");
        std::stringstream original_history_buffer;
        original_history_buffer << original_history_file.rdbuf();

        std::ifstream aux_history_file("aux_history_data");
        std::stringstream aux_history_buffer;
        aux_history_buffer << aux_history_file.rdbuf();

        ASSERT_EQ(original_history_buffer.str(), aux_history_buffer.str());

        original_history_file.close();
        aux_history_file.close();

        std::ofstream clean_aux_history_file("aux_history_file", std::ios::out | std::ios::trunc);
        clean_aux_history_file.close();
    }
}
