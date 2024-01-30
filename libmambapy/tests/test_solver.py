import random
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


@pytest.mark.parametrize(
    "attr",
    [
        "keep_dependencies",
        "keep_user_specs",
        "force_reinstall",
        "allow_downgrade",
        "allow_uninstall",
        "strict_repo_priority",
        "order_request",
    ],
)
def test_Request_Flags_boolean(attr):
    Flags = libmambapy.solver.Request.Flags

    for _ in range(10):
        val = bool(random.randint(0, 1))
        flags = Flags(**{attr: val})

        assert getattr(flags, attr) == val

        val = bool(random.randint(0, 1))
        setattr(flags, attr, val)
        assert getattr(flags, attr) == val


@pytest.mark.parametrize(
    "Action",
    [
        libmambapy.solver.Solution.Omit,
        libmambapy.solver.Solution.Reinstall,
    ],
)
def test_Solution_Action_what(Action):
    act = Action(what=libmambapy.specs.PackageInfo(name="foo"))

    assert act.what.name == "foo"

    # Setter
    act.what = libmambapy.specs.PackageInfo(name="bar")
    assert act.what.name == "bar"

    # Copy
    other = copy.deepcopy(act)
    assert other is not act
    assert other.what.name == act.what.name


@pytest.mark.parametrize(
    ("Action", "attrs"),
    [
        (libmambapy.solver.Solution.Upgrade, {"remove", "install"}),
        (libmambapy.solver.Solution.Downgrade, {"remove", "install"}),
        (libmambapy.solver.Solution.Change, {"remove", "install"}),
        (libmambapy.solver.Solution.Remove, {"remove"}),
        (libmambapy.solver.Solution.Install, {"install"}),
    ],
)
def test_Solution_Action_remove_install(Action, attrs):
    act = Action(**{a: libmambapy.specs.PackageInfo(name=a) for a in attrs})

    for a in attrs:
        assert getattr(act, a).name == a

    # Setter
    for a in attrs:
        setattr(act, a, libmambapy.specs.PackageInfo(name=f"{a}-new"))
        assert getattr(act, a).name == f"{a}-new"

    # Copy
    other = copy.deepcopy(act)
    assert other is not act
    for a in attrs:
        assert getattr(other, a).name == getattr(act, a).name


def test_Solution():
    Solution = libmambapy.solver.Solution
    PackageInfo = libmambapy.specs.PackageInfo

    actions = [
        Solution.Omit(PackageInfo(name="Omit_what")),
        Solution.Reinstall(PackageInfo(name="Reinstall_what")),
        Solution.Install(install=PackageInfo(name="Install_install")),
        Solution.Remove(remove=PackageInfo(name="Remove_remove")),
        Solution.Upgrade(
            install=PackageInfo(name="Upgrade_install"),
            remove=PackageInfo(name="Upgrade_remove"),
        ),
        Solution.Downgrade(
            install=PackageInfo(name="Downgrade_install"),
            remove=PackageInfo(name="Downgrade_remove"),
        ),
        Solution.Change(
            install=PackageInfo(name="Change_install"),
            remove=PackageInfo(name="Change_remove"),
        ),
    ]

    sol = Solution(actions)

    assert len(sol.actions) == len(actions)
    for i in range(len(actions)):
        assert isinstance(sol.actions[i], type(actions[i]))

    assert {pkg.name for pkg in sol.to_install()} == {
        "Reinstall_what",
        "Install_install",
        "Upgrade_install",
        "Downgrade_install",
        "Change_install",
    }
    assert {pkg.name for pkg in sol.to_remove()} == {
        "Reinstall_what",
        "Remove_remove",
        "Upgrade_remove",
        "Downgrade_remove",
        "Change_remove",
    }
    assert {pkg.name for pkg in sol.to_omit()} == {"Omit_what"}

    # Copy
    other = copy.deepcopy(sol)
    assert other is not sol
    assert len(other.actions) == len(sol.actions)
