#include <gtest/gtest.h>
#include <fstream>
#include <string>

#include "history.hpp"

namespace mamba
{
    TEST(history, parse)
    {
        History history_instance("history_test/");

        std::vector<History::UserRequest> user_reqs = history_instance.get_user_requests();

        std::ifstream history_file("history_test/conda-meta/history");
        std::stringstream original_history_buffer;
        std::string line;
        while (getline(history_file, line))
        {
            original_history_buffer << line;
        }
        history_file.close();

        std::ifstream src_beg("history_test/conda-meta/history", std::ios::binary);
        std::ofstream dst_beg("history_test/conda-meta/aux_file",   std::ios::binary);
        dst_beg << src_beg.rdbuf();
        src_beg.close();
        dst_beg.close();

        std::stringstream check_buffer;
        check_buffer << original_history_buffer.str() << original_history_buffer.str();

        history_instance.add_entry(user_reqs);

        history_file.open("history_test/conda-meta/history");
        std::stringstream updated_history_buffer;
        while (getline(history_file, line))
        {
            updated_history_buffer << line;
        }
        history_file.close();

        ASSERT_EQ(updated_history_buffer.str(), check_buffer.str());

        std::ofstream src_end("history_test/conda-meta/history", std::ios::binary);
        std::ifstream dst_end("history_test/conda-meta/aux_file",   std::ios::binary);
        src_end << dst_end.rdbuf();
        src_end.close();
        dst_end.close();
    }
}
