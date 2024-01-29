import libmambapy


def test_version():
    assert isinstance(libmambapy.__version__, str)
    assert libmambapy.version.__version__ == libmambapy.__version__
