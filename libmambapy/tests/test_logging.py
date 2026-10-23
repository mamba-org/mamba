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
    
    def start_log_handling(*args):
        pass

    def stop_log_handling():
        pass

    def set_log_level(*args):
        pass

    def set_params(*args):
        pass
        
    def log(*args):
        pass

    def enable_backtrace(*args):
        pass
    
    def log_backtrace():
        pass
        
    def log_backtrace_no_guards():
        pass

    def flush(*args):
        pass
    
    def set_flush_threshold(*args):
        pass
        
class LogHandlerInterfaceValidation:
    def __init__(self, enabled:bool):
        self._enabled = enabled
        self._previous_value = False

    def __enter__(self):
        import libmambapy.logging as logging
        self._previous_value = logging.enable_log_handler_interface_validation(self._enabled)

    def __exit__(self, *args):
        import libmambapy.logging as logging
        logging.enable_log_handler_interface_validation(self._previous_value)


    
class NotALogHandler:
    pass

def test_log_handler_interface_checks():
    import libmambapy.logging as logging


    log_handler = TestLogHandler()
    not_log_handler = NotALogHandler()

    # Note: order is important here, we test the throwing function before the predicate
    # so that if the test fails we get some useful info instead of assertion is False

    logging.validate_log_handler_interface(log_handler) # do not throw
    with pytest.raises(ValueError): # must throw
        logging.validate_log_handler_interface(not_log_handler)
    
    assert logging.has_log_handler_interface(log_handler)
    assert not logging.has_log_handler_interface(not_log_handler)



def test_log_handler_basics():
    import libmambapy.logging as logging

    no_log_handler = logging.AnyLogHandler()
    assert not no_log_handler
    assert not no_log_handler.has_pyobject()

    log_handler = logging.AnyLogHandler(TestLogHandler())
    assert log_handler
    assert log_handler.has_pyobject()

    with pytest.raises(ValueError), LogHandlerInterfaceValidation(True):
        should_fail = logging.AnyLogHandler(NotALogHandler())
    


def test_logging_api_basics():
    import libmambapy.logging as logging

    log_handler = TestLogHandler()
    params = logging.LoggingParams(logging_level = logging.LogLevel.DEBUG)

    previous_log_handler = logging.stop_logging()
    with LogHandlerInterfaceValidation(True):
        no_log_handler = logging.set_log_handler(log_handler, params, [ logging.LogSource.TESTS ])
        assert not no_log_handler

    last_log_handler = logging.stop_logging() # returns an `AnyLogHandler` wrapping `log_handler`
    assert last_log_handler.has_pyobject()
    assert last_log_handler.get_pyobject() == log_handler

