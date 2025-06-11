// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include <mutex>
#include <shared_mutex>

#include "mamba/util/synchronized_value.hpp"

namespace mamba::util
{
    static_assert(BasicLockable<std::mutex>);
    static_assert(BasicLockable<std::recursive_mutex>);
    static_assert(BasicLockable<std::shared_mutex>);

    static_assert(Lockable<std::mutex>);
    static_assert(Lockable<std::recursive_mutex>);
    static_assert(Lockable<std::shared_mutex>);


    static_assert(Mutex<std::mutex>);
    static_assert(Mutex<std::recursive_mutex>);
    static_assert(Mutex<std::shared_mutex>);
}

// TODO: parametrize with a various set of basic types

namespace {
    TEST_CASE("synchronized_value")
    {
        SECTION("default constructible")
        {
            mamba::util::synchronized_value<int> a;
        }

        SECTION("basic value access")
        {
            mamba::util::synchronized_value<int> a{ 42 };
            REQUIRE(a.value() == 42);
        }
    }
}