import copy

import pytest

import libmambapy.solver


def test_import_submodule():
    import libmambapy.solver as solver

    # Dummy execution
    _r = solver.Request


def test_import_recursive():
    import libmambapy as mamba

    # Dummy execution
    _r = mamba.solver.Request


@pytest.mark.parametrize(
    "Item",
    [
        libmambapy.solver.Request.Install,
        libmambapy.solver.Request.Remove,
        libmambapy.solver.Request.Update,
        libmambapy.solver.Request.Keep,
        libmambapy.solver.Request.Freeze,
        libmambapy.solver.Request.Pin,
    ],
)
def test_Request_Item_spec(Item):
    itm = Item(spec=libmambapy.specs.MatchSpec.parse("foo"))

    assert str(itm.spec) == "foo"

    # Setter
    itm.spec = libmambapy.specs.MatchSpec.parse("bar")
    assert str(itm.spec) == "bar"

    # Copy
    other = copy.deepcopy(itm)
    assert other is not itm
    assert str(other.spec) == str(itm.spec)


@pytest.mark.parametrize(
    ["Item", "kwargs"],
    [
        (libmambapy.solver.Request.Remove, {"spec": libmambapy.specs.MatchSpec.parse("foo")}),
        (libmambapy.solver.Request.UpdateAll, {}),
    ],
)
def test_Request_Item_clean(Item, kwargs):
    itm = Item(**kwargs, clean_dependencies=False)

    assert not itm.clean_dependencies

    # Setter
    itm.clean_dependencies = True
    assert itm.clean_dependencies

    # Copy
    other = copy.deepcopy(itm)
    assert other is not itm
    assert other.clean_dependencies == itm.clean_dependencies
