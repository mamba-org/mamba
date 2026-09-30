import pytest

import libmambapy


def test_log_level():
    import libmambapy.logging as logging

    # TODO: check every values
    log_level = libmambapy.logging.LogLevel.DEBUG
    assert log_level.name == "DEBUG"
    assert logging.name_of(log_level) == "debug"

    # TODO: check comparisons

def test_log_params():
    import libmambapy.logging as logging

    params = logging.LoggingParams()
    params.logging_level = libmambapy.logging.LogLevel.DEBUG
    params.log_backtrace = 42
    