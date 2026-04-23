import subprocess

import pytest

from .helpers import info


def test_log_level():
    res = info()
    assert not "debug" in res
    assert not "trace" in res
    for arg in ["--log-level=debug", "-vv"]:
        res = info(arg)
        assert "debug" in res
        assert not "trace" in res
    for arg in ["--log-level=trace", "-vvv"]:
        res = info(arg)
        assert "trace" in res
