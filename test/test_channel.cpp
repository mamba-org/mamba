#include <gtest/gtest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/channel_internal.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"

#include "mamba/api/install.hpp"

namespace mamba
{
    std::string fix_win_path(const std::string& path);

#ifdef __linux__
    std::string platform("linux-64");
#elif __APPLE__ && __x86_64__
    std::string platform("osx-64");
#elif __APPLE__ && __arm64__
    std::string platform("osx-arm64");
#elif _WIN32
    std::string platform("win-64");
#endif

#ifdef _WIN32
    TEST(Channel, fix_win_path)
    {
        std::string test_str("file://\\unc\\path\\on\\win");
        auto out = fix_win_path(test_str);
        EXPECT_EQ(out, "file:///unc/path/on/win");
        auto out2 = fix_win_path("file://C:\\Program\\ (x74)\\Users\\hello\\ world");
        EXPECT_EQ(out2, "file://C:/Program\\ (x74)/Users/hello\\ world");
        auto out3 = fix_win_path("file://\\\\Programs\\xyz");
        EXPECT_EQ(out3, "file://Programs/xyz");
    }
#endif

    TEST(ChannelContext, init)
    {
        // ChannelContext builds its custom channels with
        // make_simple_channel

        const auto& ch = ChannelContext::instance().get_channel_alias();
        EXPECT_EQ(ch.scheme(), "https");
        EXPECT_EQ(ch.location(), "conda.anaconda.org");
        EXPECT_EQ(ch.name(), "<alias>");
        EXPECT_EQ(ch.canonical_name(), "<alias>");

        const auto& custom = ChannelContext::instance().get_custom_channels();

        auto it = custom.find("pkgs/main");
        EXPECT_NE(it, custom.end());
        EXPECT_EQ(it->second.name(), "pkgs/main");
        EXPECT_EQ(it->second.location(), "repo.anaconda.com");
        EXPECT_EQ(it->second.canonical_name(), "defaults");

        it = custom.find("pkgs/pro");
        EXPECT_NE(it, custom.end());
        EXPECT_EQ(it->second.name(), "pkgs/pro");
        EXPECT_EQ(it->second.location(), "repo.anaconda.com");
        EXPECT_EQ(it->second.canonical_name(), "pkgs/pro");

        it = custom.find("pkgs/r");
        EXPECT_NE(it, custom.end());
        EXPECT_EQ(it->second.name(), "pkgs/r");
        EXPECT_EQ(it->second.location(), "repo.anaconda.com");
        EXPECT_EQ(it->second.canonical_name(), "defaults");
    }

    TEST(Channel, make_channel)
    {
        std::string value = "conda-forge";
        const Channel& c = make_channel(value);
        EXPECT_EQ(c.scheme(), "https");
        EXPECT_EQ(c.location(), "conda.anaconda.org");
        EXPECT_EQ(c.name(), "conda-forge");
        EXPECT_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));

        std::string value2 = "https://repo.anaconda.com/pkgs/main[" + platform + "]";
        const Channel& c2 = make_channel(value2);
        EXPECT_EQ(c2.scheme(), "https");
        EXPECT_EQ(c2.location(), "repo.anaconda.com");
        EXPECT_EQ(c2.name(), "pkgs/main");
        EXPECT_EQ(c2.platforms(), std::vector<std::string>({ platform }));

        std::string value3 = "https://conda.anaconda.org/conda-forge[" + platform + "]";
        const Channel& c3 = make_channel(value3);
        EXPECT_EQ(c3.scheme(), c.scheme());
        EXPECT_EQ(c3.location(), c.location());
        EXPECT_EQ(c3.name(), c.name());
        EXPECT_EQ(c3.platforms(), std::vector<std::string>({ platform }));

        std::string value4 = "/home/mamba/test/channel_b";
        const Channel& c4 = make_channel(value4);
        EXPECT_EQ(c4.scheme(), "file");
#ifdef _WIN32
        std::string driveletter = fs::absolute(fs::path("/")).string().substr(0, 1);
        EXPECT_EQ(c4.location(), driveletter + ":/home/mamba/test");
#else
        EXPECT_EQ(c4.location(), "/home/mamba/test");
#endif
        EXPECT_EQ(c4.name(), "channel_b");
        EXPECT_EQ(c4.platforms(), std::vector<std::string>({ platform, "noarch" }));

        std::string value5 = "/home/mamba/test/channel_b[" + platform + "]";
        const Channel& c5 = make_channel(value5);
        EXPECT_EQ(c5.scheme(), "file");
#ifdef _WIN32
        EXPECT_EQ(c5.location(), driveletter + ":/home/mamba/test");
#else
        EXPECT_EQ(c5.location(), "/home/mamba/test");
#endif
        EXPECT_EQ(c5.name(), "channel_b");
        EXPECT_EQ(c5.platforms(), std::vector<std::string>({ platform }));

        std::string value6a = "http://localhost:8000/conda-forge[noarch]";
        const Channel& c6a = make_channel(value6a);
        EXPECT_EQ(c6a.urls(false),
                  std::vector<std::string>({ "http://localhost:8000/conda-forge/noarch" }));

        std::string value6b = "http://localhost:8000/conda_mirror/conda-forge[noarch]";
        const Channel& c6b = make_channel(value6b);
        EXPECT_EQ(
            c6b.urls(false),
            std::vector<std::string>({ "http://localhost:8000/conda_mirror/conda-forge/noarch" }));

        std::string value7 = "conda-forge[noarch,arbitrary]";
        const Channel& c7 = make_channel(value7);
        EXPECT_EQ(c7.platforms(), std::vector<std::string>({ "noarch", "arbitrary" }));
    }

    TEST(Channel, urls)
    {
        std::string value = "https://conda.anaconda.org/conda-forge[noarch,win-64,arbitrary]";
        const Channel& c = make_channel(value);
        EXPECT_EQ(c.urls(),
                  std::vector<std::string>({ "https://conda.anaconda.org/conda-forge/noarch",
                                             "https://conda.anaconda.org/conda-forge/win-64",
                                             "https://conda.anaconda.org/conda-forge/arbitrary" }));

        const Channel& c1 = make_channel("https://conda.anaconda.org/conda-forge");
        EXPECT_EQ(c1.urls(),
                  std::vector<std::string>({ "https://conda.anaconda.org/conda-forge/" + platform,
                                             "https://conda.anaconda.org/conda-forge/noarch" }));
    }

    TEST(Channel, add_token)
    {
        auto& ctx = Context::instance();
        ctx.channel_tokens["https://conda.anaconda.org"] = "my-12345-token";

        ChannelInternal::clear_cache();

        const auto& chan = make_channel("conda-forge[noarch]");
        EXPECT_EQ(chan.token(), "my-12345-token");
        EXPECT_EQ(chan.urls(true),
                  std::vector<std::string>{
                      { "https://conda.anaconda.org/t/my-12345-token/conda-forge/noarch" } });
        EXPECT_EQ(chan.urls(false),
                  std::vector<std::string>{ { "https://conda.anaconda.org/conda-forge/noarch" } });
    }

    TEST(Channel, load_tokens)
    {
        // touch(env::home_directory() / ".continuum" / "anaconda")
        // auto& ctx = Context::instance();
        // ctx.channel_tokens["https://conda.anaconda.org"] = "my-12345-token";

        // Channel::clear_cache();

        // const auto& chan = make_channel("conda-forge");
        // EXPECT_EQ(chan.token(), "my-12345-token");
        // EXPECT_EQ(chan.url(true),
        // "https://conda.anaconda.org/t/my-12345-token/conda-forge/noarch");
        // EXPECT_EQ(chan.url(false), "https://conda.anaconda.org/conda-forge/noarch");
    }
}  // namespace mamba
