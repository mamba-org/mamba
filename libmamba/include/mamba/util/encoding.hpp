// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_ENCODING_HPP
#define MAMBA_UTIL_ENCODING_HPP

#include <cstddef>

namespace mamba::util
{
    void bytes_to_hex_to(const std::byte* first, const std::byte* last, char* out);
}
#endif
