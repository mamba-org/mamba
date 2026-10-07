import pytest

import libmambapy


def test_log_level():
    import libmambapy.logging as logging

    # TODO: check every values
    log_level = libmambapy.logging.LogLevel.DEBUG
    assert log_level.name == "DEBUG"
    assert logging.name_of_level(log_level) == "debug"

    # TODO: check comparisons

def test_log_params():
    import libmambapy.logging as logging

    params = logging.LoggingParams()
    params.logging_level = libmambapy.logging.LogLevel.DEBUG
    params.log_backtrace = 42

class TestLogHandler:
    pass


def test_log_handler_basics():
    import libmambapy.logging as logging

    no_log_handler = logging.AnyLogHandler()
    assert not no_log_handler

    log_handler = logging.AnyLogHandler(TestLogHandler())
    assert log_handler



def test_logging_api_basics():
    import libmambapy.logging as logging

    log_handler = TestLogHandler()
    params = logging.LoggingParams(logging_level = logging.LogLevel.DEBUG)

    previous_log_handler = logging.stop_logging()
    no_log_handler = logging.set_log_handler(logging.AnyLogHandler(log_handler), params, [ logging.LogSource.TESTS ])
    assert not no_log_handler
    
