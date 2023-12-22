// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include "mamba/core/output.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    std::string python_pin(PrefixData& prefix_data, const std::vector<std::string>& specs)
    {
        std::string pin = "";
        std::string py_version;

        auto iter = prefix_data.records().find("python");
        if (iter != prefix_data.records().end())
        {
            py_version = iter->second.version;
        }
        else
        {
            return "";  // Python not found in prefix
        }

        for (const auto& spec : specs)
        {
            if (MatchSpec::parse(spec).name() == "python")
            {
                return "";
            }
        }

        std::vector<std::string> elems = util::split(py_version, ".");
        std::string py_pin = util::concat("python ", elems[0], ".", elems[1], ".*");
        LOG_DEBUG << "Pinning Python to '" << py_pin << "'";
        return py_pin;
    }

    std::vector<std::string> file_pins(const fs::u8path& file)
    {
        std::vector<std::string> pins;

        if (fs::exists(file) && !fs::is_directory(file))
        {
            std::ifstream input_file(file.std_path());
            std::string line;
            while (std::getline(input_file, line))
            {
                pins.push_back(line);
            }
        }

        return pins;
    }
}
