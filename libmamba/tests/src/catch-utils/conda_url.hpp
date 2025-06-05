// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.
#pragma once

#include <string>

#include <catch2/catch_tostring.hpp>

#include "mamba/specs/conda_url.hpp"

namespace Catch
{
    template <>
    struct StringMaker<mamba::specs::CondaURL>
    {
        static std::string convert(const mamba::specs::CondaURL& value)
        {
            return value.str();
        }
    };
}
