// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/pinning.hpp"

#include <fstream>


namespace mamba
{
    std::string python_pin(const PrefixData& prefix_data, const std::vector<std::string>& specs)
    {
        std::string pin = "";
        std::string py_version;
        MatchSpec ms;

        auto iter = prefix_data.records().find("python");
        if (iter != prefix_data.records().end())
        {
            py_version = iter->second.version;
        }
        else
        {
            return "";  // Python not found in prefix
        }

        for (auto spec : specs)
        {
            ms = spec;
            if (ms.name == "python")
            {
                return "";
            }
        }

        return "python=" + py_version;
    }

    std::vector<std::string> file_pins(const fs::path& file)
    {
        std::vector<std::string> pins;

        if (fs::exists(file) && !fs::is_directory(file))
        {
            std::ifstream input_file(file);
            std::string line;
            while (std::getline(input_file, line))
            {
                pins.push_back(line);
            }
        }

        return pins;
    }
}
