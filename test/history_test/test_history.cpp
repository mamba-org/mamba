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

        std::ifstream  src("../history_test/conda-meta/history", std::ios::binary);
        std::ofstream  dst("../history_test/conda-meta/aux_file",   std::ios::binary);

        dst << src.rdbuf();
        src.close();
        dst.close();

        history_file.close();

        std::stringstream check_buffer;
        check_buffer << original_history_buffer.str() << original_history_buffer.str();

        history_instance.add_entry(user_reqs);

        history_file.open("../history_test/conda-meta/history");
        std::stringstream updated_history_buffer;
        while (getline(history_file, line))
        {
            updated_history_buffer << line;
        }

        std::cout << updated_history_buffer.str() << std::endl;
        ASSERT_EQ(updated_history_buffer.str(), check_buffer.str());

        history_file.close();

        std::ofstream  src_out("../history_test/conda-meta/history", std::ios::binary);
        std::ifstream  dst_out("../history_test/conda-meta/aux_file",   std::ios::binary);

        src_out << dst_out.rdbuf();
        src.close();
        dst_out.close();
    }
}
