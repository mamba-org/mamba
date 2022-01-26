#include <gtest/gtest.h>

#include <sstream>
#include <tuple>

#include "mamba/core/progress_bar.hpp"


namespace mamba
{
    class progress_bar : public ::testing::Test
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

    TEST_F(progress_bar, print)
    {
        auto& r = proxy.repr();

        EXPECT_TRUE(r.prefix.active());
        EXPECT_EQ(r.prefix.value(), "conda-forge");
        EXPECT_EQ(r.prefix.width(), 11);

        EXPECT_TRUE(r.progress);
        EXPECT_EQ(r.progress.value(), "??");
        EXPECT_EQ(r.progress.width(), 2);

        EXPECT_TRUE(r.separator);
        EXPECT_EQ(r.separator.value(), "-");
        EXPECT_EQ(r.separator.width(), 1);

        EXPECT_TRUE(r.total);
        EXPECT_EQ(r.total.value(), "bar");
        EXPECT_EQ(r.total.width(), 3);

        EXPECT_TRUE(r.speed);
        EXPECT_EQ(r.speed.value(), "@10");
        EXPECT_EQ(r.speed.width(), 3);

        EXPECT_TRUE(r.postfix.active());
        EXPECT_EQ(r.postfix.value(), "downloading");
        EXPECT_EQ(r.postfix.width(), 11);

        EXPECT_TRUE(r.elapsed.active());
        EXPECT_EQ(r.elapsed.value(), "0.1s");
        EXPECT_EQ(r.elapsed.width(), 4);

        proxy.print(ostream, 0, false);
        EXPECT_EQ(ostream.str(), "conda-forge ?? foo - bar @10 downloading 0.1s");
        ostream.str("");

        r.set_width(21);  // no impact if 'update_repr' not called
        proxy.print(ostream, 0, false);
        EXPECT_EQ(ostream.str(), "conda-forge ?? foo - bar @10 downloading 0.1s");
        ostream.str("");
    }

    TEST_F(progress_bar, print_no_resize)
    {
        auto& r = proxy.repr();

        r.set_width(150);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_TRUE(r.separator);
        EXPECT_TRUE(r.total);
        EXPECT_TRUE(r.speed);
        EXPECT_TRUE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 106);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.separator.width(), 1);
        EXPECT_EQ(r.total.width(), 3);
        EXPECT_EQ(r.speed.width(), 3);
        EXPECT_EQ(r.postfix.width(), 11);
        EXPECT_EQ(r.elapsed.width(), 5);
    }

    TEST_F(progress_bar, print_reduce_bar)
    {
        auto& r = proxy.repr();

        r.set_width(84);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_TRUE(r.separator);
        EXPECT_TRUE(r.total);
        EXPECT_TRUE(r.speed);
        EXPECT_TRUE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 40);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.separator.width(), 1);
        EXPECT_EQ(r.total.width(), 3);
        EXPECT_EQ(r.speed.width(), 3);
        EXPECT_EQ(r.postfix.width(), 11);
        EXPECT_EQ(r.elapsed.width(), 5);

        // 1: reduce bar width
        // available space redistributed to the bar
        r.set_width(83);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_TRUE(r.separator);
        EXPECT_TRUE(r.total);
        EXPECT_TRUE(r.speed);
        EXPECT_TRUE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 39);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.separator.width(), 1);
        EXPECT_EQ(r.total.width(), 3);
        EXPECT_EQ(r.speed.width(), 3);
        EXPECT_EQ(r.postfix.width(), 11);
        EXPECT_EQ(r.elapsed.width(), 5);
    }

    TEST_F(progress_bar, print_remove_total_sep)
    {
        auto& r = proxy.repr();

        r.set_width(59);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_TRUE(r.separator);
        EXPECT_TRUE(r.total);
        EXPECT_TRUE(r.speed);
        EXPECT_TRUE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 15);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.separator.width(), 1);
        EXPECT_EQ(r.total.width(), 3);
        EXPECT_EQ(r.speed.width(), 3);
        EXPECT_EQ(r.postfix.width(), 11);
        EXPECT_EQ(r.elapsed.width(), 5);

        // 2: remove the total value and the separator
        // available space redistributed to the bar
        r.set_width(58);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_FALSE(r.separator);
        EXPECT_FALSE(r.total);
        EXPECT_TRUE(r.speed);
        EXPECT_TRUE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 20);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.speed.width(), 3);
        EXPECT_EQ(r.postfix.width(), 11);
        EXPECT_EQ(r.elapsed.width(), 5);
    }

    TEST_F(progress_bar, print_remove_speed)
    {
        auto& r = proxy.repr();

        r.set_width(53);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_FALSE(r.separator);
        EXPECT_FALSE(r.total);
        EXPECT_TRUE(r.speed);
        EXPECT_TRUE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 15);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.speed.width(), 3);
        EXPECT_EQ(r.postfix.width(), 11);
        EXPECT_EQ(r.elapsed.width(), 5);

        // 3: remove the speed
        // available space redistributed to the bar
        r.set_width(52);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_FALSE(r.separator);
        EXPECT_FALSE(r.total);
        EXPECT_FALSE(r.speed);
        EXPECT_TRUE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 18);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.postfix.width(), 11);
        EXPECT_EQ(r.elapsed.width(), 5);
    }

    TEST_F(progress_bar, print_remove_postfix)
    {
        auto& r = proxy.repr();

        r.set_width(49);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_FALSE(r.separator);
        EXPECT_FALSE(r.total);
        EXPECT_FALSE(r.speed);
        EXPECT_TRUE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 15);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.postfix.width(), 11);
        EXPECT_EQ(r.elapsed.width(), 5);

        // 4: remove the postfix
        // available space redistributed to the bar
        r.set_width(48);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_FALSE(r.separator);
        EXPECT_FALSE(r.total);
        EXPECT_FALSE(r.speed);
        EXPECT_FALSE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 26);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.elapsed.width(), 5);
    }

    TEST_F(progress_bar, print_truncate_prefix)
    {
        auto& r = proxy.repr();
        proxy.set_prefix("some_very_very_long_prefix");

        r.set_width(52);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_FALSE(r.separator);
        EXPECT_FALSE(r.total);
        EXPECT_FALSE(r.speed);
        EXPECT_FALSE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 26);
        EXPECT_EQ(r.progress.width(), 15);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.elapsed.width(), 5);

        // 5: truncate the prefix if too long
        // available space redistributed to the prefix
        r.set_width(51);
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_FALSE(r.separator);
        EXPECT_FALSE(r.total);
        EXPECT_FALSE(r.speed);
        EXPECT_FALSE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 25);
        EXPECT_EQ(r.progress.width(), 15);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_EQ(r.elapsed.width(), 5);
    }

    TEST_F(progress_bar, print_without_bar)
    {
        auto& r = proxy.repr();

        r.set_width(34).reset_fields();
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_TRUE(r.current);
        EXPECT_FALSE(r.separator);
        EXPECT_FALSE(r.total);
        EXPECT_FALSE(r.speed);
        EXPECT_FALSE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 12);
        EXPECT_EQ(r.current.width(), 3);
        EXPECT_TRUE(r.progress.overflow());
        EXPECT_EQ(r.elapsed.width(), 5);

        // 6: display progress without a bar
        r.set_width(33);
        proxy.update_repr();
        proxy.print(ostream, 0, false);
        EXPECT_EQ(ostream.str(), "conda-forge          0% foo    --");
        ostream.str("");
    }

    TEST_F(progress_bar, print_remove_current)
    {
        auto& r = proxy.repr();

        r.set_width(26).reset_fields();
        proxy.update_repr();
        proxy.print(ostream, 0, false);
        EXPECT_EQ(ostream.str(), "conda-forge   0% foo    --");
        ostream.str("");

        // 7: remove the current value
        r.set_width(25).reset_fields();
        proxy.update_repr();
        proxy.print(ostream, 0, false);
        EXPECT_EQ(ostream.str(), "conda-forge      0%    --");
        ostream.str("");
    }

    TEST_F(progress_bar, print_remove_elapsed)
    {
        auto& r = proxy.repr();

        r.set_width(22).reset_fields();
        proxy.update_repr();
        EXPECT_TRUE(r.prefix);
        EXPECT_TRUE(r.progress);
        EXPECT_FALSE(r.current);
        EXPECT_FALSE(r.separator);
        EXPECT_FALSE(r.total);
        EXPECT_FALSE(r.speed);
        EXPECT_FALSE(r.postfix);
        EXPECT_TRUE(r.elapsed);
        proxy.print(ostream, 0, false);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 4);
        EXPECT_EQ(r.elapsed.width(), 5);
        EXPECT_EQ(ostream.str(), "conda-forge   0%    --");
        ostream.str("");

        // 8: remove the elapsed time
        r.set_width(21);
        proxy.update_repr();
        proxy.print(ostream, 0, false);
        EXPECT_EQ(r.prefix.width(), 11);
        EXPECT_EQ(r.progress.width(), 9);
        EXPECT_EQ(ostream.str(), "conda-forge        0%");
        ostream.str("");
    }
}  // namespace mamba
