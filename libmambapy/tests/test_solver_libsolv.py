import copy

import pytest

import libmambapy.solver.libsolv as libsolv


def test_import_submodule():
    import libmambapy.solver.libsolv as solv

    # Dummy execution
    _p = solv.Priorities


def test_import_recursive():
    import libmambapy as mamba

    # Dummy execution
    _p = mamba.solver.libsolv.Priorities


def test_RepodataParser():
    assert libsolv.RepodataParser.Mamba.name == "Mamba"
    assert libsolv.RepodataParser.Libsolv.name == "Libsolv"

    assert libsolv.RepodataParser("Libsolv") == libsolv.RepodataParser.Libsolv

    with pytest.raises(KeyError):
        libsolv.RepodataParser("NoParser")


def test_PipASPythonDependency():
    assert libsolv.PipAsPythonDependency.No.name == "No"
    assert libsolv.PipAsPythonDependency.Yes.name == "Yes"

    assert libsolv.PipAsPythonDependency(True) == libsolv.PipAsPythonDependency.Yes


def test_Priorities():
    p = libsolv.Priorities(priority=-1, subpriority=-2)

    assert p.priority == -1
    assert p.subpriority == -2

    # Setters
    p.priority = 33
    p.subpriority = 0
    assert p.priority == 33
    assert p.subpriority == 0

    # Operators
    assert p == p
    assert p != libsolv.Priorities()

    # Copy
    other = copy.deepcopy(p)
    assert other is not p
    assert other == p
