// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <sstream>

#include <catch2/catch_all.hpp>

#include "mamba/core/progress_bar.hpp"

#include "../src/core/progress_bar_impl.hpp"

#include "mambatests.hpp"

namespace mamba
{
    class progress_bar
    {
    public:

        progress_bar()
        {
            const auto& context = mambatests::context();
            p_progress_bar_manager = std::make_unique<MultiBarManager>();
            proxy = p_progress_bar_manager->add_progress_bar(
                "conda-forge",
                { context.graphics_params, context.ascii_only }
            );

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

    namespace
    {
        TEST_CASE_FIXTURE(progress_bar, "print")
        {
            auto& r = proxy.repr();

            REQUIRE(r.prefix.active());
            REQUIRE(r.prefix.value() == "conda-forge");
            REQUIRE(r.prefix.width() == 11);

            REQUIRE(r.progress);
            REQUIRE(r.progress.value() == "??");
            REQUIRE(r.progress.width() == 2);

            REQUIRE(r.separator);
            REQUIRE(r.separator.value() == "-");
            REQUIRE(r.separator.width() == 1);

            REQUIRE(r.total);
            REQUIRE(r.total.value() == "bar");
            REQUIRE(r.total.width() == 3);

            REQUIRE(r.speed);
            REQUIRE(r.speed.value() == "@10");
            REQUIRE(r.speed.width() == 3);

            REQUIRE(r.postfix.active());
            REQUIRE(r.postfix.value() == "downloading");
            REQUIRE(r.postfix.width() == 11);

            REQUIRE(r.elapsed.active());
            REQUIRE(r.elapsed.value() == "0.1s");
            REQUIRE(r.elapsed.width() == 4);

            proxy.print(ostream, 0, false);
            REQUIRE(ostream.str() == "conda-forge ?? foo - bar @10 downloading 0.1s");
            ostream.str("");

            r.set_width(21);  // no impact if 'update_repr' not called
            proxy.print(ostream, 0, false);
            REQUIRE(ostream.str() == "conda-forge ?? foo - bar @10 downloading 0.1s");
            ostream.str("");
        }

        TEST_CASE_FIXTURE(progress_bar, "print_no_resize")
        {
            auto& r = proxy.repr();

            r.set_width(150);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE(r.separator);
            REQUIRE(r.total);
            REQUIRE(r.speed);
            REQUIRE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 106);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.separator.width() == 1);
            REQUIRE(r.total.width() == 3);
            REQUIRE(r.speed.width() == 3);
            REQUIRE(r.postfix.width() == 11);
            REQUIRE(r.elapsed.width() == 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_reduce_bar")
        {
            auto& r = proxy.repr();

            r.set_width(84);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE(r.separator);
            REQUIRE(r.total);
            REQUIRE(r.speed);
            REQUIRE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 40);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.separator.width() == 1);
            REQUIRE(r.total.width() == 3);
            REQUIRE(r.speed.width() == 3);
            REQUIRE(r.postfix.width() == 11);
            REQUIRE(r.elapsed.width() == 5);

            // 1: reduce bar width
            // available space redistributed to the bar
            r.set_width(83);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE(r.separator);
            REQUIRE(r.total);
            REQUIRE(r.speed);
            REQUIRE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 39);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.separator.width() == 1);
            REQUIRE(r.total.width() == 3);
            REQUIRE(r.speed.width() == 3);
            REQUIRE(r.postfix.width() == 11);
            REQUIRE(r.elapsed.width() == 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_remove_total_sep")
        {
            auto& r = proxy.repr();

            r.set_width(59);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE(r.separator);
            REQUIRE(r.total);
            REQUIRE(r.speed);
            REQUIRE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 15);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.separator.width() == 1);
            REQUIRE(r.total.width() == 3);
            REQUIRE(r.speed.width() == 3);
            REQUIRE(r.postfix.width() == 11);
            REQUIRE(r.elapsed.width() == 5);

            // 2: remove the total value and the separator
            // available space redistributed to the bar
            r.set_width(58);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE_FALSE(r.separator);
            REQUIRE_FALSE(r.total);
            REQUIRE(r.speed);
            REQUIRE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 20);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.speed.width() == 3);
            REQUIRE(r.postfix.width() == 11);
            REQUIRE(r.elapsed.width() == 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_remove_speed")
        {
            auto& r = proxy.repr();

            r.set_width(53);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE_FALSE(r.separator);
            REQUIRE_FALSE(r.total);
            REQUIRE(r.speed);
            REQUIRE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 15);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.speed.width() == 3);
            REQUIRE(r.postfix.width() == 11);
            REQUIRE(r.elapsed.width() == 5);

            // 3: remove the speed
            // available space redistributed to the bar
            r.set_width(52);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE_FALSE(r.separator);
            REQUIRE_FALSE(r.total);
            REQUIRE_FALSE(r.speed);
            REQUIRE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 18);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.postfix.width() == 11);
            REQUIRE(r.elapsed.width() == 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_remove_postfix")
        {
            auto& r = proxy.repr();

            r.set_width(49);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE_FALSE(r.separator);
            REQUIRE_FALSE(r.total);
            REQUIRE_FALSE(r.speed);
            REQUIRE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 15);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.postfix.width() == 11);
            REQUIRE(r.elapsed.width() == 5);

            // 4: remove the postfix
            // available space redistributed to the bar
            r.set_width(48);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE_FALSE(r.separator);
            REQUIRE_FALSE(r.total);
            REQUIRE_FALSE(r.speed);
            REQUIRE_FALSE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 26);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.elapsed.width() == 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_truncate_prefix")
        {
            auto& r = proxy.repr();
            proxy.set_prefix("some_very_very_long_prefix");

            r.set_width(52);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE_FALSE(r.separator);
            REQUIRE_FALSE(r.total);
            REQUIRE_FALSE(r.speed);
            REQUIRE_FALSE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 26);
            REQUIRE(r.progress.width() == 15);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.elapsed.width() == 5);

            // 5: truncate the prefix if too long
            // available space redistributed to the prefix
            r.set_width(51);
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE_FALSE(r.separator);
            REQUIRE_FALSE(r.total);
            REQUIRE_FALSE(r.speed);
            REQUIRE_FALSE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 25);
            REQUIRE(r.progress.width() == 15);
            REQUIRE(r.current.width() == 3);
            REQUIRE(r.elapsed.width() == 5);
        }

        TEST_CASE_FIXTURE(progress_bar, "print_without_bar")
        {
            auto& r = proxy.repr();

            r.set_width(34).reset_fields();
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE(r.current);
            REQUIRE_FALSE(r.separator);
            REQUIRE_FALSE(r.total);
            REQUIRE_FALSE(r.speed);
            REQUIRE_FALSE(r.postfix);
            REQUIRE(r.elapsed);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 12);
            REQUIRE(r.current.width() == 3);
            // This fails because of invisible ANSI escape codes introduced with
            // https://github.com/mamba-org/mamba/pull/2085/
            // REQUIRE(r.progress.overflow());
            REQUIRE(r.elapsed.width() == 5);

            // 6: display progress without a bar
            r.set_width(33);
            proxy.update_repr();
            proxy.print(ostream, 0, false);
            REQUIRE(ostream.str() == "conda-forge          0% foo    --");
            ostream.str("");
        }

        TEST_CASE_FIXTURE(progress_bar, "print_remove_current")
        {
            auto& r = proxy.repr();

            r.set_width(26).reset_fields();
            proxy.update_repr();
            proxy.print(ostream, 0, false);
            REQUIRE(ostream.str() == "conda-forge   0% foo    --");
            ostream.str("");

            // 7: remove the current value
            r.set_width(25).reset_fields();
            proxy.update_repr();
            proxy.print(ostream, 0, false);
            REQUIRE(ostream.str() == "conda-forge      0%    --");
            ostream.str("");
        }

        TEST_CASE_FIXTURE(progress_bar, "print_remove_elapsed")
        {
            auto& r = proxy.repr();

            r.set_width(22).reset_fields();
            proxy.update_repr();
            REQUIRE(r.prefix);
            REQUIRE(r.progress);
            REQUIRE_FALSE(r.current);
            REQUIRE_FALSE(r.separator);
            REQUIRE_FALSE(r.total);
            REQUIRE_FALSE(r.speed);
            REQUIRE_FALSE(r.postfix);
            REQUIRE(r.elapsed);
            proxy.print(ostream, 0, false);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 4);
            REQUIRE(r.elapsed.width() == 5);
            REQUIRE(ostream.str() == "conda-forge   0%    --");
            ostream.str("");

            // 8: remove the elapsed time
            r.set_width(21);
            proxy.update_repr();
            proxy.print(ostream, 0, false);
            REQUIRE(r.prefix.width() == 11);
            REQUIRE(r.progress.width() == 9);
            REQUIRE(ostream.str() == "conda-forge        0%");
            ostream.str("");
        }
    }
}  // namespace mamba
