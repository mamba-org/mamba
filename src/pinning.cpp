// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/pinning.hpp"

#include <fstream>


namespace mamba
{
    void pin_python_spec(const PrefixData& prefix_data, std::vector<std::string>& specs)
    {
        bool py_found = false;
        std::string py_spec = "";

        for (auto spec : specs)
        {
            if (spec.find("python") != std::string::npos)
            {
                py_found = true;
                py_spec = spec;
                break;
            }
        }

        if (py_found)
        {
            MatchSpec ms(py_spec);
            std::regex ver_pat("=?[0-9]+\\.[0-9]+.*");

            if (std::regex_match(ms.version, ver_pat))
            {
                return;
            }
        }

        try
        {
            std::string py_version = prefix_data.records().at("python").version;
            specs.erase(std::remove(specs.begin(), specs.end(), py_spec), specs.end());
            specs.push_back("python=" + py_version);
            return;
        }
        catch (const std::exception& e)
        {
            return;
        }
    }

    void pin_config_specs(const std::vector<std::string>& config_specs,
                          std::vector<std::string>& specs)
    {
        specs.insert(specs.end(), config_specs.begin(), config_specs.end());
    }

    void pin_file_specs(const fs::path& file_specs, std::vector<std::string>& specs)
    {
        if (fs::exists(file_specs) && !fs::is_directory(file_specs))
        {
            std::ifstream input_file(file_specs);
            std::string line;
            while (std::getline(input_file, line))
            {
                specs.push_back(line);
            }
        }
    }
}
