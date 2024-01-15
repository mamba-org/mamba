// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/util/random.hpp"

namespace mamba ::util
{
    auto random_generator() -> default_random_generator
    {
        return random_generator<default_random_generator>();
    }

    auto local_random_generator() -> default_random_generator&
    {
        return local_random_generator<default_random_generator>();
    }

    auto generate_random_alphanumeric_string(std::size_t len) -> std::string
    {
        return generate_random_alphanumeric_string(len, local_random_generator());
    }
}
