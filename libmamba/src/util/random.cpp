// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/util/random.hpp"

namespace mamba ::util
{
    template auto random_generator<default_random_generator>() -> default_random_generator;

    template auto local_random_generator<default_random_generator>() -> default_random_generator&;

    template auto generate_random_alphanumeric_string<default_random_generator>(
        std::size_t len,
        default_random_generator& generator
    ) -> std::string;
}
