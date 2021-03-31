// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_INFO_HPP
#define UMAMBA_INFO_HPP

#include "mamba/core/context.hpp"

#ifdef VENDORED_CLI11
#include "mamba/core/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif

#include <string>
#include <tuple>
#include <vector>


const char banner[] = R"MAMBARAW(
                                           __
          __  ______ ___  ____ _____ ___  / /_  ____ _
         / / / / __ `__ \/ __ `/ __ `__ \/ __ \/ __ `/
        / /_/ / / / / / / /_/ / / / / / / /_/ / /_/ /
       / .___/_/ /_/ /_/\__,_/_/ /_/ /_/_.___/\__,_/
      /_/
)MAMBARAW";

void
init_info_parser(CLI::App* subcom);

void
set_info_command(CLI::App* subcom);

void
info_pretty_print(std::vector<std::tuple<std::string, std::vector<std::string>>> map);

std::string
version();

#endif
