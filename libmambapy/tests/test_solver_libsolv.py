import copy

import libmambapy.solver.libsolv as libsolv


def test_import_submodule():
    import libmambapy.solver.libsolv as solv

    # Dummy execution
    _p = solv.Priorities


def test_import_recursive():
    import libmambapy as mamba

    # Dummy execution
    _p = mamba.solver.libsolv.Priorities


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
