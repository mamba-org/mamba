// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <forward_list>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include "mamba/util/iterator.hpp"

using namespace mamba::util;

namespace
{
    bool greater_than_10(int i)
    {
        return i > 10;
    }

    struct greater_than_10_obj
    {
        bool operator()(int i)
        {
            return i > 10;
        }
    };

    struct filter_test_data
    {
        filter_test_data()
            : input_forward_sequence({ 1, 12, 2, 3, 14, 4, 18, 20, 4 })
            , res_forward_sequence({ 12, 14, 18, 20 })
            , input_bidirectional_sequence({ 1, 12, 2, 3, 14, 4, 18, 20, 4 })
            , res_bidirectional_sequence({ 12, 14, 18, 20 })
            , input_random_sequence({ 1, 12, 2, 3, 14, 4, 18, 20, 4 })
            , res_random_sequence({ 12, 14, 18, 20 })
        {
        }

        std::forward_list<int> input_forward_sequence;
        std::forward_list<int> res_forward_sequence;
        std::list<int> input_bidirectional_sequence;
        std::list<int> res_bidirectional_sequence;
        std::vector<int> input_random_sequence;
        std::vector<int> res_random_sequence;
    };
}

template <class Seq, class Pred>
void
test_forward_api(Seq& input, const Seq& res, Pred p)
{
    auto f = filter(input, p);

    auto iter = f.begin();
    auto iter_end = f.end();

    auto citer = f.cbegin();
    auto citer_end = f.cend();

    auto res_iter = res.begin();
    auto res_iter_end = res.end();
    auto iter2 = iter, iter3 = iter;

    while (res_iter != res_iter_end)
    {
        CHECK_EQ(*iter, *res_iter);
        CHECK_EQ(*iter, *citer);
        ++iter, ++citer, ++res_iter;
    }
    CHECK_EQ(iter, iter_end);
    CHECK_EQ(citer, citer_end);
    CHECK(iter == iter_end);

    CHECK_EQ(iter2.operator->(), (++input.begin()).operator->());
    CHECK(iter2++ != ++iter3);
}

template <class Seq, class Pred>
void
test_bidirectional_api(Seq& input, const Seq& res, Pred pred)
{
    auto f = filter(input, pred);

    auto iter = f.begin();
    auto iter_end = f.end();

    auto citer = f.cbegin();
    auto citer_end = f.cend();

    auto res_iter = res.begin();
    auto res_iter_end = res.end();
    auto iter2 = iter_end, iter3 = iter_end;

    while (res_iter_end != res_iter)
    {
        --iter_end, --citer_end, --res_iter_end;
        CHECK_EQ(*iter_end, *res_iter_end);
        CHECK_EQ(*iter_end, *citer_end);
    }

    CHECK_EQ(iter, iter_end);
    CHECK_EQ(citer, citer_end);
    --iter_end, --citer_end;

    CHECK_EQ(iter2.operator->(), input.end().operator->());
    CHECK(iter2-- != --iter3);
}

template <class Seq, class Pred>
void
test_random_access_api(Seq& input, const Seq& res, Pred pred)
{
    auto f = filter(input, pred);

    auto iter = f.begin();
    auto iter_end = f.end();

    auto citer = f.cbegin();
    auto citer_end = f.cend();

    auto res_iter = res.begin();

    auto iter2 = iter;
    auto citer2 = citer;
    auto res_iter2 = res_iter;

    CHECK_EQ(iter_end - iter, static_cast<std::ptrdiff_t>(res.size()));
    CHECK_EQ(citer_end - citer, static_cast<std::ptrdiff_t>(res.size()));

    CHECK_EQ(*(iter + 2), *(res_iter + 2));
    CHECK_EQ(*(citer + 2), *(res_iter + 2));
    iter += 2, res_iter += 2, citer += 2;
    CHECK_EQ(*iter, *res_iter);
    CHECK_EQ(*citer, *res_iter);

    CHECK_EQ(iter2[1], res_iter2[1]);
    CHECK_EQ(citer2[1], res_iter2[1]);
}

TEST_SUITE("util::filter_iterator")
{
    TEST_CASE("forward_iterator_api")
    {
        filter_test_data data;
        test_forward_api(data.input_forward_sequence, data.res_forward_sequence, greater_than_10);
        test_forward_api(
            data.input_bidirectional_sequence,
            data.res_bidirectional_sequence,
            greater_than_10
        );
        test_forward_api(data.input_random_sequence, data.res_random_sequence, greater_than_10);

        test_forward_api(data.input_forward_sequence, data.res_forward_sequence, greater_than_10_obj{});
        test_forward_api(
            data.input_bidirectional_sequence,
            data.res_bidirectional_sequence,
            greater_than_10_obj{}
        );
        test_forward_api(data.input_random_sequence, data.res_random_sequence, greater_than_10_obj{});
    }

    TEST_CASE("bidirectional_iterator_api")
    {
        filter_test_data data;
        test_bidirectional_api(
            data.input_bidirectional_sequence,
            data.res_bidirectional_sequence,
            greater_than_10
        );
        test_bidirectional_api(data.input_random_sequence, data.res_random_sequence, greater_than_10);
        test_bidirectional_api(
            data.input_bidirectional_sequence,
            data.res_bidirectional_sequence,
            greater_than_10_obj{}
        );
        test_bidirectional_api(
            data.input_random_sequence,
            data.res_random_sequence,
            greater_than_10_obj{}
        );
    }

    TEST_CASE("random_access_iterator_api")
    {
        filter_test_data data;
        test_random_access_api(data.input_random_sequence, data.res_random_sequence, greater_than_10);
        test_random_access_api(
            data.input_random_sequence,
            data.res_random_sequence,
            greater_than_10_obj{}
        );
    }
}
