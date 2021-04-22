#include <gtest/gtest.h>

#include "mamba/api/install.hpp"

namespace mamba
{
    TEST(env_file_reading, selector)
    {
    	if (on_linux || on_mac)
    	{
	    	EXPECT_TRUE(eval_selector("sel(unix)"));
	    	if (on_mac)
	    	{
	    		EXPECT_TRUE(eval_selector("sel(osx)"));
	    		EXPECT_FALSE(eval_selector("sel(linux)"));
	    		EXPECT_FALSE(eval_selector("sel(win)"));
	    	}
	    	else
	    	{
	    		EXPECT_TRUE(eval_selector("sel(linux)"));
	    		EXPECT_FALSE(eval_selector("sel(osx)"));
	    		EXPECT_FALSE(eval_selector("sel(win)"));
	    	}
    	}
    	else if (on_win)
    	{
    		EXPECT_TRUE(eval_selector("sel(win)"));
    		EXPECT_FALSE(eval_selector("sel(osx)"));
    		EXPECT_FALSE(eval_selector("sel(linux)"));
    	}
    }
}  // namespace mamba
