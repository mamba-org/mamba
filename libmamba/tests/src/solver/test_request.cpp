// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <type_traits>

#include <catch2/catch_all.hpp>

#include "mamba/solver/request.hpp"
#include "mamba/specs/match_spec.hpp"

using namespace mamba;
using namespace mamba::solver;

namespace
{
    using namespace specs::match_spec_literals;

    TEST_CASE("Create a request", "[mamba::solver]")
    {
        auto request = Request{
            {},
            {
                Request::Install{ "a>1.2"_ms },
                Request::Remove{ "b>1.2"_ms, true },
                Request::UpdateAll{},
                Request::Freeze{ "c"_ms },
                Request::Pin{ "d"_ms },
                Request::Install{ "a>1.0"_ms },
            },
        };

        SECTION("Iterate over same elements")
        {
            auto count_install = std::size_t(0);
            for_each_of<Request::Install>(request, [&](const Request::Install&) { count_install++; });
            REQUIRE(count_install == 2);
        }

        SECTION("Iterate over different elements")
        {
            auto count_install = std::size_t(0);
            auto count_remove = std::size_t(0);
            for_each_of<Request::Install, Request::Remove>(
                request,
                [&](const auto& itm)
                {
                    using Itm = std::decay_t<decltype(itm)>;
                    if constexpr (std::is_same_v<Itm, Request::Install>)
                    {
                        count_install++;
                    }
                    else if constexpr (std::is_same_v<Itm, Request::Remove>)
                    {
                        count_remove++;
                    }
                }
            );

            REQUIRE(count_install == 2);
            REQUIRE(count_remove == 1);
        }

        SECTION("Iterate over elements and break loop")
        {
            auto count_install = std::size_t(0);
            for_each_of<Request::Install>(
                request,
                [&](const Request::Install&)
                {
                    count_install++;
                    return util::LoopControl::Break;
                }
            );
            REQUIRE(count_install == 1);
        }
    }
}
