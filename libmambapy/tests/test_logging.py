import pytest

import libmambapy


def test_log_level_basics():
    import libmambapy.logging as logging

    # TODO: check every values
    log_level = libmambapy.LogLevel.DEBUG
    assert log_level.name == "DEBUG"
    assert logging.name_of(log_level) == "debug"
