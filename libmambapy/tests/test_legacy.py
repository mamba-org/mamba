import libmambapy


def test_channel_context():
    ctx = libmambapy.Context.instance()

    cc = libmambapy.ChannelContext.make_conda_compatible(ctx)
    assert cc.make_channel("pkgs/main")[0].url.str() == "https://repo.anaconda.com/pkgs/main"
    assert "pkgs/main" in cc.params().custom_channels
    chan = cc.params().custom_channels["pkgs/main"]
    assert isinstance(cc.has_zst(chan), bool)  # Not testing value

    cc = libmambapy.ChannelContext.make_simple(ctx)
    assert cc.make_channel("pkgs/main")[0].url.str() == "https://conda.anaconda.org/pkgs/main"
    assert len(cc.params().custom_channels) == 0
