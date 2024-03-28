// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_OS_UNIX_HPP
#define MAMBA_UTIL_OS_UNIX_HPP

#include <string>
#include <utility>

#include <tl/expected.hpp>

#include "mamba/util/os.hpp"

namespace mamba::util
{
    [[nodiscard]] auto unix_name_version()
        -> tl::expected<std::pair<std::string, std::string>, OSError>;
}
#endif
