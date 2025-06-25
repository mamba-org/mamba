// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <atomic>
#include <concepts>
#include <future>
#include <memory>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <thread>

#include <catch2/catch_all.hpp>

#include "mamba/util/synchronized_value.hpp"

#include "mambatests_utils.hpp"

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


    static_assert(SharedMutex<std::shared_mutex>);

    // Scope locked must be possible to move in different scopes without unlocking-relocking,
    // so it is imperative that they are moveable, but should not be copyable.
    static_assert(std::move_constructible<scoped_locked_ptr<std::unique_ptr<int>, std::mutex, true>>);
    static_assert(std::move_constructible<
                  scoped_locked_ptr<std::unique_ptr<int>, std::recursive_mutex, true>>);
    static_assert(std::move_constructible<scoped_locked_ptr<std::unique_ptr<int>, std::shared_mutex, true>>);
    static_assert(std::move_constructible<scoped_locked_ptr<std::unique_ptr<int>, std::mutex, false>>);
    static_assert(std::move_constructible<
                  scoped_locked_ptr<std::unique_ptr<int>, std::recursive_mutex, false>>);
    static_assert(std::move_constructible<
                  scoped_locked_ptr<std::unique_ptr<int>, std::shared_mutex, false>>);
    static_assert(std::is_nothrow_move_constructible_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::mutex, true>>);
    static_assert(std::is_nothrow_move_constructible_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::recursive_mutex, true>>);
    static_assert(std::is_nothrow_move_constructible_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::shared_mutex, true>>);
    static_assert(std::is_nothrow_move_constructible_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::mutex, false>>);
    static_assert(std::is_nothrow_move_constructible_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::recursive_mutex, false>>);
    static_assert(std::is_nothrow_move_constructible_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::shared_mutex, false>>);
    static_assert(std::is_nothrow_move_assignable_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::mutex, true>>);
    static_assert(std::is_nothrow_move_assignable_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::recursive_mutex, true>>);
    static_assert(std::is_nothrow_move_assignable_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::shared_mutex, true>>);
    static_assert(std::is_nothrow_move_assignable_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::mutex, false>>);
    static_assert(std::is_nothrow_move_assignable_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::recursive_mutex, false>>);
    static_assert(std::is_nothrow_move_assignable_v<
                  scoped_locked_ptr<std::unique_ptr<int>, std::shared_mutex, false>>);

}

namespace
{

    struct ValueType
    {
        int x = 0;

        auto increment() -> void
        {
            x++;
        }

        auto next_value() const -> ValueType
        {
            return { x + 1 };
        }

        auto operator<=>(const ValueType&) const noexcept = default;
    };

    // NOTE: We do not use TEMPLATE_TEST_CASE or TEMPLATE_LIST_TEST_CASE here because code coverage
    // tools (such as gcov/lcov) do not properly attribute coverage to tests instantiated via
    // template test cases. Instead, we use individual TEST_CASEs for each mutex type, and factorize
    // the test logic into function templates to avoid code duplication. This ensures accurate code
    // coverage reporting.

    using supported_mutex_types = std::tuple<std::mutex, std::shared_mutex, std::recursive_mutex>;

    template <mamba::util::Mutex MutexType>
    void test_synchronized_value_basics()
    {
        using synchronized_value = mamba::util::synchronized_value<ValueType, MutexType>;

        SECTION("default constructible")
        {
            synchronized_value a;
        }

        static constexpr auto initial_value = ValueType{ 42 };
        synchronized_value sv{ initial_value };

        SECTION("value access and assignation")
        {
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);

            const auto& const_sv = std::as_const(sv);
            REQUIRE(const_sv.unsafe_get() == initial_value);
            REQUIRE(const_sv.value() == initial_value);
            REQUIRE(const_sv == initial_value);
            REQUIRE(const_sv->x == initial_value.x);

            sv->increment();
            const auto expected_new_value = initial_value.next_value();
            REQUIRE(sv.unsafe_get() == expected_new_value);
            REQUIRE(sv.value() == expected_new_value);
            REQUIRE(sv == expected_new_value);
            REQUIRE(sv != initial_value);
            REQUIRE(sv->x == expected_new_value.x);
            REQUIRE(const_sv.unsafe_get() == expected_new_value);
            REQUIRE(const_sv.value() == expected_new_value);
            REQUIRE(const_sv == expected_new_value);
            REQUIRE(const_sv != initial_value);
            REQUIRE(const_sv->x == expected_new_value.x);

            sv = initial_value;
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv != expected_new_value);
            REQUIRE(sv->x == initial_value.x);
            REQUIRE(const_sv.unsafe_get() == initial_value);
            REQUIRE(const_sv.value() == initial_value);
            REQUIRE(const_sv == initial_value);
            REQUIRE(const_sv != expected_new_value);
            REQUIRE(const_sv->x == initial_value.x);
        }

        SECTION("value access using synchronize")
        {
            sv = initial_value;
            {
                auto sync_sv = std::as_const(sv).synchronize();
                REQUIRE(*sync_sv == initial_value);
                REQUIRE(sync_sv->x == initial_value.x);
            }
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);

            static constexpr auto expected_value = ValueType{ 12345 };
            {
                auto sync_sv = sv.synchronize();
                sync_sv->x = expected_value.x;
            }
            REQUIRE(sv.unsafe_get() == expected_value);
            REQUIRE(sv.value() == expected_value);
            REQUIRE(sv == expected_value);
            REQUIRE(sv->x == expected_value.x);

            {
                auto sync_sv = sv.synchronize();
                *sync_sv = initial_value;
            }
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);
        }

        SECTION("value access using apply")
        {
            sv = initial_value;
            {
                auto result = std::as_const(sv).apply([](const ValueType& value) { return value.x; });
                REQUIRE(result == initial_value.x);
            }
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);

            static constexpr auto expected_value = ValueType{ 98765 };
            sv.apply([](ValueType& value) { value = expected_value; });
            REQUIRE(sv.unsafe_get() == expected_value);
            REQUIRE(sv.value() == expected_value);
            REQUIRE(sv == expected_value);
            REQUIRE(sv->x == expected_value.x);

            sv.apply([](ValueType& value, auto new_value) { value = new_value; }, initial_value);
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);
        }
    }

    TEST_CASE("synchronized_value basics with std::mutex", "[thread-safe]")
    {
        test_synchronized_value_basics<std::mutex>();
    }

    TEST_CASE("synchronized_value basics with std::shared_mutex", "[thread-safe]")
    {
        test_synchronized_value_basics<std::shared_mutex>();
    }

    TEST_CASE("synchronized_value basics with std::recursive_mutex", "[thread-safe]")
    {
        test_synchronized_value_basics<std::recursive_mutex>();
    }

    // Factorized initializer-list test
    template <mamba::util::Mutex MutexType>
    void test_synchronized_value_initializer_list()
    {
        using synchronized_value = mamba::util::synchronized_value<std::vector<int>, MutexType>;
        synchronized_value values{ 1, 2, 3, 4 };
    }

    // Factorized apply example test
    template <mamba::util::Mutex MutexType>
    void test_synchronized_value_apply_example()
    {
        using synchronized_value = mamba::util::synchronized_value<std::vector<int>, MutexType>;
        const std::vector initial_values{ 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
        const std::vector sorted_values{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        synchronized_value values{ initial_values };
        values.apply(std::ranges::sort);
        REQUIRE(values == sorted_values);
        values.apply(std::ranges::sort, std::ranges::greater{});
        REQUIRE(values == initial_values);
    }

    template <mamba::util::Mutex M>
    auto test_concurrent_increment(
        std::invocable<mamba::util::synchronized_value<ValueType, M>&> auto increment_task
    )
    {
        static constexpr auto arbitrary_number_of_executing_threads = 512;

        mamba::util::synchronized_value<ValueType, M> current_value;
        static constexpr int expected_result = arbitrary_number_of_executing_threads;

        std::atomic<bool> run_tasks = false;  // used to launch tasks about the same time, simpler
                                              // than condition_variable
        std::vector<std::future<void>> tasks;

        // Launch the reading and writing tasks (maybe threads, depends on how async is implemented)
        for (int i = 0; i < expected_result * 2; ++i)
        {
            if (i % 2)  // intertwine reading and writing tasks
            {
                // add writing task
                tasks.push_back(std::async(
                    std::launch::async,
                    [&, increment_task]
                    {
                        // don't actually run until we get the green light
                        mambatests::wait_condition([&] { return run_tasks == true; });
                        increment_task(current_value);
                    }
                ));
            }
            else
            {
                // add reading task
                tasks.push_back(std::async(
                    std::launch::async,
                    [&]
                    {
                        // don't actually run until we get the green light
                        mambatests::wait_condition([&] { return run_tasks == true; });
                        const auto& readonly_value = std::as_const(current_value);
                        static constexpr auto arbitrary_read_count = 100;
                        long long sum = 0;
                        for (int c = 0; c < arbitrary_read_count; ++c)
                        {
                            sum += readonly_value->x;   // TODO: also try to mix reading and writing
                                                        // using different kinds of access
                            std::this_thread::yield();  // for timing randomness and limit
                                                        // over-exhaustion
                        }
                        REQUIRE(sum != 0);  // It is possible but extremely unlikely that all
                                            // reading tasks will read before any writing tasks.
                    }
                ));
            }
        }

        run_tasks = true;  // green light, tasks will run probably concurrently, worse case in
                           // unpredictable order
        for (auto& task : tasks)
        {
            task.wait();  // wait all to be finished
        }

        REQUIRE(current_value->x == expected_result);
    }

    // Factorized thread-safe direct_access test
    template <mamba::util::Mutex MutexType>
    void test_synchronized_value_threadsafe_direct_access()
    {
        using synchronized_value = mamba::util::synchronized_value<ValueType, MutexType>;
        test_concurrent_increment<MutexType>([](synchronized_value& sv) { sv->x += 1; });
    }

    // Factorized thread-safe synchronize test
    template <mamba::util::Mutex MutexType>
    void test_synchronized_value_threadsafe_synchronize()
    {
        using synchronized_value = mamba::util::synchronized_value<ValueType, MutexType>;
        test_concurrent_increment<MutexType>(
            [](synchronized_value& sv)
            {
                auto synched_sv = sv.synchronize();
                synched_sv->x += 1;
            }
        );
    }

    // Factorized thread-safe apply test
    template <mamba::util::Mutex MutexType>
    void test_synchronized_value_threadsafe_apply()
    {
        using synchronized_value = mamba::util::synchronized_value<ValueType, MutexType>;
        test_concurrent_increment<MutexType>([](synchronized_value& sv)
                                             { sv.apply([](ValueType& value) { value.x += 1; }); });
    }

    // Factorized thread-safe multiple synchronize test
    template <mamba::util::Mutex MutexType>
    void test_synchronized_value_threadsafe_multiple_synchronize()
    {
        using synchronized_value = mamba::util::synchronized_value<ValueType, MutexType>;
        const mamba::util::synchronized_value<std::vector<int>, std::shared_mutex> extra_values{ 1 };
        test_concurrent_increment<MutexType>(
            [&](synchronized_value& sv)
            {
                auto [ssv, sev] = synchronize(sv, extra_values);
                ssv->x += sev->front();
            }
        );
    }

    // Individual test cases for each mutex type
    TEST_CASE("synchronized_value initializer-list with std::mutex", "[thread-safe]")
    {
        test_synchronized_value_initializer_list<std::mutex>();
    }

    TEST_CASE("synchronized_value initializer-list with std::shared_mutex", "[thread-safe]")
    {
        test_synchronized_value_initializer_list<std::shared_mutex>();
    }

    TEST_CASE("synchronized_value initializer-list with std::recursive_mutex", "[thread-safe]")
    {
        test_synchronized_value_initializer_list<std::recursive_mutex>();
    }

    TEST_CASE("synchronized_value apply example with std::mutex", "[thread-safe]")
    {
        test_synchronized_value_apply_example<std::mutex>();
    }

    TEST_CASE("synchronized_value apply example with std::shared_mutex", "[thread-safe]")
    {
        test_synchronized_value_apply_example<std::shared_mutex>();
    }

    TEST_CASE("synchronized_value apply example with std::recursive_mutex", "[thread-safe]")
    {
        test_synchronized_value_apply_example<std::recursive_mutex>();
    }

    TEST_CASE("synchronized_value thread-safe direct_access with std::mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_direct_access<std::mutex>();
    }

    TEST_CASE("synchronized_value thread-safe direct_access with std::shared_mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_direct_access<std::shared_mutex>();
    }

    TEST_CASE("synchronized_value thread-safe direct_access with std::recursive_mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_direct_access<std::recursive_mutex>();
    }

    TEST_CASE("synchronized_value thread-safe synchronize with std::mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_synchronize<std::mutex>();
    }

    TEST_CASE("synchronized_value thread-safe synchronize with std::shared_mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_synchronize<std::shared_mutex>();
    }

    TEST_CASE("synchronized_value thread-safe synchronize with std::recursive_mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_synchronize<std::recursive_mutex>();
    }

    TEST_CASE("synchronized_value thread-safe apply with std::mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_apply<std::mutex>();
    }

    TEST_CASE("synchronized_value thread-safe apply with std::shared_mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_apply<std::shared_mutex>();
    }

    TEST_CASE("synchronized_value thread-safe apply with std::recursive_mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_apply<std::recursive_mutex>();
    }

    TEST_CASE("synchronized_value thread-safe multiple synchronize with std::mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_multiple_synchronize<std::mutex>();
    }

    TEST_CASE("synchronized_value thread-safe multiple synchronize with std::shared_mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_multiple_synchronize<std::shared_mutex>();
    }

    TEST_CASE("synchronized_value thread-safe multiple synchronize with std::recursive_mutex", "[thread-safe]")
    {
        test_synchronized_value_threadsafe_multiple_synchronize<std::recursive_mutex>();
    }

    TEST_CASE("synchronized_value basics multiple synchronize")
    {
        using namespace mamba::util;
        // mutables
        synchronized_value<ValueType> a{ ValueType{ 1 } };
        synchronized_value<ValueType, std::recursive_mutex> b{ ValueType{ 3 } };
        synchronized_value<ValueType, std::shared_mutex> c{ ValueType{ 5 } };
        synchronized_value<std::vector<int>> d{ 7 };
        synchronized_value<std::vector<int>, std::recursive_mutex> e{ 9 };
        synchronized_value<std::vector<int>, std::shared_mutex> f{ 11 };

        // immutables (readonly)
        const synchronized_value<ValueType> ca{ ValueType{ 2 } };
        const synchronized_value<ValueType, std::recursive_mutex> cb{ ValueType{ 4 } };
        const synchronized_value<ValueType, std::shared_mutex> cc{ ValueType{ 6 } };
        const synchronized_value<std::vector<int>> cd{ 8 };
        const synchronized_value<std::vector<int>, std::recursive_mutex> ce{ 10 };
        const synchronized_value<std::vector<int>, std::shared_mutex> cf{ 12 };

        std::vector<int> values;
        {
            auto [sa, sca, sb, scb, sc, scc, sd, scd, se, sce, sf, scf] = mamba::util::
                synchronize(a, ca, b, cb, c, cc, d, cd, e, ce, f, cf);
            static_assert(std::same_as<decltype(sa), scoped_locked_ptr<ValueType, std::mutex, false>>);
            static_assert(std::same_as<decltype(sca), scoped_locked_ptr<ValueType, std::mutex, true>>);
            static_assert(std::same_as<decltype(sb), scoped_locked_ptr<ValueType, std::recursive_mutex, false>>);
            static_assert(std::same_as<decltype(scb), scoped_locked_ptr<ValueType, std::recursive_mutex, true>>);
            static_assert(std::same_as<decltype(sc), scoped_locked_ptr<ValueType, std::shared_mutex, false>>);
            static_assert(std::same_as<decltype(scc), scoped_locked_ptr<ValueType, std::shared_mutex, true>>);
            static_assert(std::same_as<decltype(sd), scoped_locked_ptr<std::vector<int>, std::mutex, false>>);
            static_assert(std::same_as<decltype(scd), scoped_locked_ptr<std::vector<int>, std::mutex, true>>);
            static_assert(std::same_as<
                          decltype(se),
                          scoped_locked_ptr<std::vector<int>, std::recursive_mutex, false>>);
            static_assert(std::same_as<
                          decltype(sce),
                          scoped_locked_ptr<std::vector<int>, std::recursive_mutex, true>>);
            static_assert(std::same_as<
                          decltype(sf),
                          scoped_locked_ptr<std::vector<int>, std::shared_mutex, false>>);
            static_assert(std::same_as<
                          decltype(scf),
                          scoped_locked_ptr<std::vector<int>, std::shared_mutex, true>>);

            values.push_back(sa->x);
            values.push_back(sca->x);
            values.push_back(sb->x);
            values.push_back(scb->x);
            values.push_back(sc->x);
            values.push_back(scc->x);
            values.push_back(sd->front());
            values.push_back(scd->front());
            values.push_back(se->front());
            values.push_back(sce->front());
            values.push_back(sf->front());
            values.push_back(scf->front());
        }
        std::ranges::sort(values);
        REQUIRE(values == std::vector{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 });
    }
}
