// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_CONSTRUCTOR_HPP
#define UMAMBA_CONSTRUCTOR_HPP

#include "mamba/core/mamba_fs.hpp"


void
construct(const fs::path& prefix, bool extract_conda_pkgs, bool extract_tarball);

void
read_binary_from_stdin_and_write_to_file(fs::path& filename);

#endif
