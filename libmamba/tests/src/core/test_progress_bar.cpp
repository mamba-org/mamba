// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <sstream>

#include <doctest/doctest.h>

#include "mamba/core/progress_bar.hpp"

#include "../src/core/progress_bar_impl.hpp"

namespace mamba
{
    class progress_bar
    {
    public:

        progress_bar()
        {
            p_progress_bar_manager = std::make_unique<MultiBarManager>();
            proxy = p_progress_bar_manager->add_progress_bar("conda-forge");

            auto& r = proxy.repr();
            r.progress.set_value("??");
            r.current.set_value("foo");
            r.separator.set_value("-");
            r.total.set_value("bar");
            r.speed.set_value("@10");
            r.postfix.set_value("downloading");
            r.elapsed.set_value("0.1s");
        }

    protected:

        std::unique_ptr<ProgressBarManager> p_progress_bar_manager;
        ProgressProxy proxy;
        std::ostringstream ostream;
    };

    TEST_SUITE("progress_bar")
    {
        TEST_CASE_FIXTURE(progress_bar, "print")
        {
            auto& r = proxy.repr();

            CHECK(r.prefix.active());
            CHECK_EQ(r.prefix.value(), "conda-forge");
            CHECK_EQ(r.prefix.width(), 11);

            CHECK(r.progress);
            CHECK_EQ(r.progress.value(), "??");
            CHECK_EQ(r.progress.width(), 2);

            CHECK(r.separator);
            CHECK_EQ(r.separator.value(), "-");
            CHECK_EQ(r.separator.width(), 1);

            CHECK(r.total);
            CHECK_EQ(r.total.value(), "bar");
            CHECK_EQ(r.total.width(), 3);

            CHECK(r.speed);
            CHECK_EQ(r.speed.value(), "@10");
            CHECK_EQ(r.speed.width(), 3);

            CHECK(r.postfix.active());
            CHECK_EQ(r.postfix.value(), "downloading");
            CHECK_EQ(r.postfix.width(), 11);

            CHECK(r.elapsed.active());
            CHECK_EQ(r.elapsed.value(), "0.1s");
            CHECK_EQ(r.elapsed.width(), 4);

            proxy.print(ostream, 0, false);
            CHECK_EQ(ostream.str(), "conda-forge ?? foo - bar @10 downloading 0.1s");
            ostream.str("");

            r.set_width(21);  // no impact if 'update_repr' not called
            proxy.print(ostream, 0, false);
            CHECK_EQ(ostream.str(), "conda-forge ?? foo - bar @10 downloading 0.1s");
            ostream.str("");
        }

        TEST_CASE_FIXTURE(progress_bar, "print_no_resize")
        {
            auto& r = proxy.repr();

            r.set_width(150);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK(r.separator);
            CHECK(r.total);
            CHECK(r.speed);
            CHECK(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 106);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.separator.width(), 1);
            CHECK_EQ(r.total.width(), 3);
            CHECK_EQ(r.speed.width(), 3);
            CHECK_EQ(r.postfix.width(), 11);
            CHECK_EQ(r.elapsed.width(), 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_reduce_bar")
        {
            auto& r = proxy.repr();

            r.set_width(84);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK(r.separator);
            CHECK(r.total);
            CHECK(r.speed);
            CHECK(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 40);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.separator.width(), 1);
            CHECK_EQ(r.total.width(), 3);
            CHECK_EQ(r.speed.width(), 3);
            CHECK_EQ(r.postfix.width(), 11);
            CHECK_EQ(r.elapsed.width(), 5);

            // 1: reduce bar width
            // available space redistributed to the bar
            r.set_width(83);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK(r.separator);
            CHECK(r.total);
            CHECK(r.speed);
            CHECK(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 39);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.separator.width(), 1);
            CHECK_EQ(r.total.width(), 3);
            CHECK_EQ(r.speed.width(), 3);
            CHECK_EQ(r.postfix.width(), 11);
            CHECK_EQ(r.elapsed.width(), 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_remove_total_sep")
        {
            auto& r = proxy.repr();

            r.set_width(59);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK(r.separator);
            CHECK(r.total);
            CHECK(r.speed);
            CHECK(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 15);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.separator.width(), 1);
            CHECK_EQ(r.total.width(), 3);
            CHECK_EQ(r.speed.width(), 3);
            CHECK_EQ(r.postfix.width(), 11);
            CHECK_EQ(r.elapsed.width(), 5);

            // 2: remove the total value and the separator
            // available space redistributed to the bar
            r.set_width(58);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK_FALSE(r.separator);
            CHECK_FALSE(r.total);
            CHECK(r.speed);
            CHECK(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 20);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.speed.width(), 3);
            CHECK_EQ(r.postfix.width(), 11);
            CHECK_EQ(r.elapsed.width(), 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_remove_speed")
        {
            auto& r = proxy.repr();

            r.set_width(53);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK_FALSE(r.separator);
            CHECK_FALSE(r.total);
            CHECK(r.speed);
            CHECK(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 15);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.speed.width(), 3);
            CHECK_EQ(r.postfix.width(), 11);
            CHECK_EQ(r.elapsed.width(), 5);

            // 3: remove the speed
            // available space redistributed to the bar
            r.set_width(52);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK_FALSE(r.separator);
            CHECK_FALSE(r.total);
            CHECK_FALSE(r.speed);
            CHECK(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 18);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.postfix.width(), 11);
            CHECK_EQ(r.elapsed.width(), 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_remove_postfix")
        {
            auto& r = proxy.repr();

            r.set_width(49);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK_FALSE(r.separator);
            CHECK_FALSE(r.total);
            CHECK_FALSE(r.speed);
            CHECK(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 15);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.postfix.width(), 11);
            CHECK_EQ(r.elapsed.width(), 5);

            // 4: remove the postfix
            // available space redistributed to the bar
            r.set_width(48);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK_FALSE(r.separator);
            CHECK_FALSE(r.total);
            CHECK_FALSE(r.speed);
            CHECK_FALSE(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 26);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.elapsed.width(), 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_truncate_prefix")
        {
            auto& r = proxy.repr();
            proxy.set_prefix("some_very_very_long_prefix");

            r.set_width(52);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK_FALSE(r.separator);
            CHECK_FALSE(r.total);
            CHECK_FALSE(r.speed);
            CHECK_FALSE(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 26);
            CHECK_EQ(r.progress.width(), 15);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.elapsed.width(), 5);

            // 5: truncate the prefix if too long
            // available space redistributed to the prefix
            r.set_width(51);
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK_FALSE(r.separator);
            CHECK_FALSE(r.total);
            CHECK_FALSE(r.speed);
            CHECK_FALSE(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 25);
            CHECK_EQ(r.progress.width(), 15);
            CHECK_EQ(r.current.width(), 3);
            CHECK_EQ(r.elapsed.width(), 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_without_bar")
        {
            auto& r = proxy.repr();

            r.set_width(34).reset_fields();
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK(r.current);
            CHECK_FALSE(r.separator);
            CHECK_FALSE(r.total);
            CHECK_FALSE(r.speed);
            CHECK_FALSE(r.postfix);
            CHECK(r.elapsed);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 12);
            CHECK_EQ(r.current.width(), 3);
            // This fails because of invisible ANSI escape codes introduced with
            // https://github.com/mamba-org/mamba/pull/2085/
            // CHECK(r.progress.overflow());
            CHECK_EQ(r.elapsed.width(), 5);

            // 6: display progress without a bar
            r.set_width(33);
            proxy.update_repr();
            proxy.print(ostream, 0, false);
            CHECK_EQ(ostream.str(), "conda-forge          0% foo    --");
            ostream.str("");
        }

        TEST_CASE_FIXTURE(progress_bar, "print_remove_current")
        {
            auto& r = proxy.repr();

            r.set_width(26).reset_fields();
            proxy.update_repr();
            proxy.print(ostream, 0, false);
            CHECK_EQ(ostream.str(), "conda-forge   0% foo    --");
            ostream.str("");

            // 7: remove the current value
            r.set_width(25).reset_fields();
            proxy.update_repr();
            proxy.print(ostream, 0, false);
            CHECK_EQ(ostream.str(), "conda-forge      0%    --");
            ostream.str("");
        }

        TEST_CASE_FIXTURE(progress_bar, "print_remove_elapsed")
        {
            auto& r = proxy.repr();

            r.set_width(22).reset_fields();
            proxy.update_repr();
            CHECK(r.prefix);
            CHECK(r.progress);
            CHECK_FALSE(r.current);
            CHECK_FALSE(r.separator);
            CHECK_FALSE(r.total);
            CHECK_FALSE(r.speed);
            CHECK_FALSE(r.postfix);
            CHECK(r.elapsed);
            proxy.print(ostream, 0, false);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 4);
            CHECK_EQ(r.elapsed.width(), 5);
            CHECK_EQ(ostream.str(), "conda-forge   0%    --");
            ostream.str("");

            // 8: remove the elapsed time
            r.set_width(21);
            proxy.update_repr();
            proxy.print(ostream, 0, false);
            CHECK_EQ(r.prefix.width(), 11);
            CHECK_EQ(r.progress.width(), 9);
            CHECK_EQ(ostream.str(), "conda-forge        0%");
            ostream.str("");
        }
    }
}  // namespace mamba
