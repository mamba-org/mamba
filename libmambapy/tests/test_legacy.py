import subprocess
import sys
import libmambapy


def test_context_instance_scoped():
    ctx = libmambapy.Context()  # Initialize and then terminate libmamba internals
    assert ctx is not None


def test_context_no_log_nor_signal_handling():
    ctx = libmambapy.Context(
        libmambapy.ContextOptions(enable_logging=False, enable_signal_handling=False)
    )
    assert ctx is not None


def test_channel_context():
    ctx = libmambapy.Context()

    cc = libmambapy.ChannelContext.make_conda_compatible(ctx)
    assert cc.make_channel("pkgs/main")[0].url.str() == "https://repo.anaconda.com/pkgs/main"
    assert "pkgs/main" in cc.params().custom_channels
    chan = cc.params().custom_channels["pkgs/main"]
    assert isinstance(cc.has_zst(chan), bool)  # Not testing value

    cc = libmambapy.ChannelContext.make_simple(ctx)
    assert cc.make_channel("pkgs/main")[0].url.str() == "https://conda.anaconda.org/pkgs/main"
    assert len(cc.params().custom_channels) == 0


def test_context_survives_interpreter_shutdown():
    # Regression for mamba-org/mamba#4378 / conda/constructor#1319:
    # destroying Context while the process exits must not segfault.
    script = """
import libmambapy
ctx = libmambapy.Context()
assert ctx is not None
# Intentionally keep the Context alive until interpreter shutdown.
"""
    result = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
