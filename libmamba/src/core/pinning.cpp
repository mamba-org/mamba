// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include "mamba/core/output.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    std::vector<std::string>
    python_pin(PrefixData& prefix_data, const std::vector<std::string>& specs)
    {
        std::vector<std::string> pins;
        std::string py_version;

        auto iter = prefix_data.records().find("python");
        if (iter != prefix_data.records().end())
        {
            py_version = iter->second.version;
        }
        else
        {
            return pins;  // Python not found in prefix
        }

        for (const auto& spec : specs)
        {
            if (auto ms = specs::MatchSpec::parse(spec); ms && ms->name().contains("python"))
            {
                return pins;
            }
        }

        std::vector<std::string> elems = util::split(py_version, ".");
        std::string py_pin_str = util::concat("python ", elems[0], ".", elems[1], ".*");
        // Parse and use MatchSpec's string representation to ensure correct format
        auto py_pin_ms = specs::MatchSpec::parse(py_pin_str)
                             .or_else([](specs::ParseError&& err) { throw std::move(err); })
                             .value();
        std::string py_pin = py_pin_ms.conda_build_form();
        LOG_DEBUG << "Pinning Python to '" << py_pin << "'";
        pins.push_back(py_pin);

        // If python-freethreading is installed, also pin python_abi to preserve the ABI
        auto freethreading_iter = prefix_data.records().find("python-freethreading");
        if (freethreading_iter != prefix_data.records().end())
        {
            auto python_abi_iter = prefix_data.records().find("python_abi");
            if (python_abi_iter != prefix_data.records().end())
            {
                // Extract the ABI tag from the build_string (e.g., "8_cp314t" -> "*_cp314t")
                const auto& build_string = python_abi_iter->second.build_string;
                // Find the ABI pattern (starts with underscore, e.g., "_cp314t" or "_cp314")
                auto underscore_pos = build_string.find('_');
                if (underscore_pos != std::string::npos)
                {
                    std::string abi_pattern = build_string.substr(underscore_pos);
                    // Pin format: python_abi[version="=3.13",build="*_cp313t"]
                    // This preserves the ABI tag (e.g., _cp314t for free-threaded) while allowing
                    // any build number. Use attribute format to avoid ambiguity.
                    std::vector<std::string> version_elems = util::split(
                        python_abi_iter->second.version,
                        "."
                    );
                    std::string version_pin;
                    if (version_elems.size() >= 2)
                    {
                        version_pin = util::concat("=", version_elems[0], ".", version_elems[1]);
                    }
                    else
                    {
                        // Fallback if version format is unexpected
                        version_pin = util::concat("=", python_abi_iter->second.version);
                    }
                    std::string build_pin = util::concat("*", abi_pattern);
                    std::string python_abi_pin_str = util::concat(
                        "python_abi[version=\"",
                        version_pin,
                        "\",build=\"",
                        build_pin,
                        "\"]"
                    );
                    // Parse and use MatchSpec's string representation to ensure correct format
                    auto python_abi_pin_ms = specs::MatchSpec::parse(python_abi_pin_str)
                                                 .or_else([](specs::ParseError&& err)
                                                          { throw std::move(err); })
                                                 .value();
                    std::string python_abi_pin = python_abi_pin_ms.to_string();
                    LOG_DEBUG << "Pinning python_abi to '" << python_abi_pin << "' (free-threaded)";
                    pins.push_back(python_abi_pin);
                }
            }
        }

        return pins;
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
