// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_OS_LINUX_HPP
#define MAMBA_UTIL_OS_LINUX_HPP

#include <string>

#include <tl/expected.hpp>

#include "mamba/util/os.hpp"

namespace mamba::util
{
    [[nodiscard]] auto linux_version() -> tl::expected<std::string, OSError>;
}
#endif
