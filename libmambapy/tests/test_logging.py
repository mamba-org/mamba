import pytest

import libmambapy
import libmambapy.logging as logging
import inspect

def test_log_level():
    # TODO: check every values
    log_level = libmambapy.logging.LogLevel.DEBUG
    assert log_level.name == "DEBUG"
    assert logging.name_of_level(log_level) == "debug"

    # TODO: check comparisons

def test_log_params():
    params = logging.LoggingParams()
    params.logging_level = libmambapy.logging.LogLevel.DEBUG
    params.log_backtrace = 42

class TestLogHandler:
    ### LogHandler implementation designed for testing purposes.

    class Counters:
        start_log_handling = 0
        stop_log_handling = 0
        set_log_level = 0
        set_params = 0
        log = 0
        enable_backtrace = 0
        log_backtrace = 0
        log_backtrace_no_guards = 0
        flush = 0
        set_flush_threshold = 0

    _counters = Counters()
    _log_history = []
    _backtrace = []
    _backtrace_size = 0
    _params = logging.LoggingParams()


    def start_log_handling(self, params : logging.LoggingParams, sources : []):
        self._counters.start_log_handling += 1
        self._params = params

    def stop_log_handling(self):
        self._counters.stop_log_handling += 1

    def set_log_level(self, new_level: logging.LogLevel):
        self._counters.set_log_level += 1
        self._params.logging_level = new_level

    def set_params(self, params : logging.LoggingParams):
        self._counters.set_params += 1
        self._params = params
        
    def log(self, log_record: logging.LogRecord):
        self._counters.log += 1

        if self._params.logging_level == logging.LogLevel.OFF:
            return

        if self._params.logging_level == logging.LogLevel.ALL or self._params.logging_level >= log_record.level:
            # NOTE: we ignore the backtrace feature here, we are not testing this implementation but the bindings
            self._log_history.append(log_record)



    def enable_backtrace(self, new_backtrace_size: int):
        self._counters.enable_backtrace += 1
        # NOTE: we ignore the backtrace feature here, we are not testing this implementation but the bindings
    
    def log_backtrace(self):
        self._counters.log_backtrace += 1
        # NOTE: we ignore the backtrace feature here, we are not testing this implementation but the bindings
        
    def log_backtrace_no_guards(self):
        self._counters.log_backtrace_no_guards += 1
        # NOTE: we ignore the backtrace feature here, we are not testing this implementation but the bindings

    def flush(self, *args):
        self._counters.flush += 1
    
    def set_flush_threshold(self, *args):
        self._counters.set_flush_threshold += 1
        
class LogHandlerInterfaceValidation:
    ### Tool enabling log handler objects interfaces check in logigng functions
    ### for the durection of a `with` scope.

    def __init__(self, enabled:bool):
        self._enabled = enabled
        self._previous_value = False

    def __enter__(self):
        self._previous_value = logging.enable_log_handler_interface_validation(self._enabled)

    def __exit__(self, *args):
        logging.enable_log_handler_interface_validation(self._previous_value)


    
class NotALogHandler:
    pass

def test_log_handler_interface_checks():

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

    no_log_handler = logging.AnyLogHandler()
    assert not no_log_handler
    assert not no_log_handler.has_pyobject()

    log_handler = logging.AnyLogHandler(TestLogHandler())
    assert log_handler
    assert log_handler.has_pyobject()

    with pytest.raises(ValueError), LogHandlerInterfaceValidation(True):
        should_fail = logging.AnyLogHandler(NotALogHandler())
    


def test_logging_api_basics():

    log_handler = TestLogHandler()
    params = logging.LoggingParams(logging_level = logging.LogLevel.DEBUG)

    previous_log_handler = logging.stop_logging()
    with LogHandlerInterfaceValidation(True):
        no_log_handler = logging.set_log_handler(log_handler, params=params, sources=[ logging.LogSource.TESTS ])
        assert not no_log_handler

    logging.log(logging.LogRecord(message="A", level=logging.LogLevel.INFO, source=logging.LogSource.TESTS ))
    logging.log(logging.LogRecord(message="B", level=logging.LogLevel.INFO, source=logging.LogSource.TESTS, location = inspect.currentframe() ))
    logging.log(logging.LogRecord(message="C", level=logging.LogLevel.DEBUG))

    assert len(log_handler._log_history) == 2
    assert log_handler._log_history[0].message == "A"
    assert log_handler._log_history[1].message == "B"

    last_log_handler = logging.stop_logging() # returns an `AnyLogHandler` wrapping `log_handler`
    assert last_log_handler.has_pyobject()
    assert last_log_handler.get_pyobject() == log_handler


