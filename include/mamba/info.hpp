// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_INFO_HPP
#define MAMBA_INFO_HPP

#include "mamba/mamba_fs.hpp"
#include "mamba/util.hpp"

#include <string>
#include <vector>


namespace mamba
{
    void info(const fs::path& prefix = "");
    std::string version();
    std::string banner();

    const std::string mamba_banner = std::string(strip(R"MAMBARAW(
                                        __
            _____ ___  ____ _____ ___  / /_  ____ _
            / __ `__ \/ __ `/ __ `__ \/ __ \/ __ `/
           / / / / / / /_/ / / / / / / /_/ / /_/ /
          /_/ /_/ /_/\__,_/_/ /_/ /_/_.___/\__,_/
    )MAMBARAW",
                                                       "\n"));

    namespace detail
    {
        void print_info();
        void info_pretty_print(std::vector<std::tuple<std::string, std::vector<std::string>>> map);
    }
}

#endif
