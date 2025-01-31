import random
import copy

import pytest

import libmambapy


def test_import_submodule():
    import libmambapy.solver as solver

    # Dummy execution
    _r = solver.Request


def test_import_recursive():
    import libmambapy as mamba

    # Dummy execution
    _r = mamba.solver.Request


@pytest.mark.parametrize(
    "Job",
    [
        libmambapy.solver.Request.Install,
        libmambapy.solver.Request.Remove,
        libmambapy.solver.Request.Update,
        libmambapy.solver.Request.Keep,
        libmambapy.solver.Request.Freeze,
        libmambapy.solver.Request.Pin,
    ],
)
def test_Request_Job_spec(Job):
    itm = Job(spec=libmambapy.specs.MatchSpec.parse("foo"))

    assert str(itm.spec) == "foo"

    # Setter
    itm.spec = libmambapy.specs.MatchSpec.parse("bar")
    assert str(itm.spec) == "bar"

    # Copy
    other = copy.deepcopy(itm)
    assert other is not itm
    assert str(other.spec) == str(itm.spec)


@pytest.mark.parametrize(
    ["Job", "kwargs"],
    [
        (libmambapy.solver.Request.Remove, {"spec": libmambapy.specs.MatchSpec.parse("foo")}),
        (libmambapy.solver.Request.Update, {"spec": libmambapy.specs.MatchSpec.parse("foo")}),
        (libmambapy.solver.Request.UpdateAll, {}),
    ],
)
def test_Request_Job_clean(Job, kwargs):
    itm = Job(**kwargs, clean_dependencies=False)

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


def test_Request():
    Request = libmambapy.solver.Request
    MatchSpec = libmambapy.specs.MatchSpec

    request = Request(
        jobs=[Request.Install(MatchSpec.parse("foo"))],
        flags=Request.Flags(keep_dependencies=False),
    )

    # Getters
    assert len(request.jobs) == 1
    assert not request.flags.keep_dependencies

    # Setters
    request.jobs.append(Request.Remove(MatchSpec.parse("bar<2.0")))
    assert len(request.jobs) == 2
    request.flags.keep_dependencies = True
    assert request.flags.keep_dependencies

    # Copy
    other = copy.deepcopy(request)
    assert other is not request
    assert len(other.jobs) == len(request.jobs)


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


def test_ProblemsMessageFormat():
    ProblemsMessageFormat = libmambapy.solver.ProblemsMessageFormat

    format = ProblemsMessageFormat()

    format = ProblemsMessageFormat(
        available=libmambapy.utils.TextStyle(foreground="Green"),
        unavailable=libmambapy.utils.TextStyle(foreground="Red"),
        indents=["a", "b", "c", "d"],
    )

    # Getters
    assert format.available.foreground == libmambapy.utils.TextTerminalColor.Green
    assert format.unavailable.foreground == libmambapy.utils.TextTerminalColor.Red
    assert format.indents == ["a", "b", "c", "d"]

    # Setters
    format.available = libmambapy.utils.TextStyle(foreground="White")
    format.unavailable = libmambapy.utils.TextStyle(foreground="Black")
    format.indents = ["1", "2", "3", "4"]
    assert format.available.foreground == libmambapy.utils.TextTerminalColor.White
    assert format.unavailable.foreground == libmambapy.utils.TextTerminalColor.Black
    assert format.indents == ["1", "2", "3", "4"]

    # Copy
    other = copy.deepcopy(format)
    assert other is not format
    assert other.indents == format.indents


def test_ProblemsGraph():
    # Create a ProblemsGraph
    db = libmambapy.solver.libsolv.Database(libmambapy.specs.ChannelResolveParams())
    db.add_repo_from_packages(
        [
            libmambapy.specs.PackageInfo(name="a", version="1.0", depends=["b>=2.0", "c>=2.1"]),
            libmambapy.specs.PackageInfo(name="b", version="2.0", depends=["c<2.0"]),
            libmambapy.specs.PackageInfo(name="c", version="1.0"),
            libmambapy.specs.PackageInfo(name="c", version="3.0"),
        ],
    )

    request = libmambapy.solver.Request(
        [libmambapy.solver.Request.Install(libmambapy.specs.MatchSpec.parse("a"))]
    )

    outcome = libmambapy.solver.libsolv.Solver().solve(db, request)

    assert isinstance(outcome, libmambapy.solver.libsolv.UnSolvable)
    pbg = outcome.problems_graph(db)
    assert isinstance(pbg.root_node(), int)

    # ProblemsGraph conflicts
    conflicts = pbg.conflicts()
    assert len(conflicts) == 2
    assert len(list(conflicts)) == 2
    node, in_conflict = next(iter(conflicts))
    assert conflicts.has_conflict(node)
    for other in in_conflict:
        assert conflicts.in_conflict(node, other)

    other_conflicts = copy.deepcopy(conflicts)
    assert other_conflicts is not conflicts
    assert other_conflicts == conflicts

    other_conflicts.clear()
    assert len(other_conflicts) == 0

    other_conflicts.add(7, 42)
    assert other_conflicts.in_conflict(7, 42)

    # ProblemsGraph graph
    nodes, edges = pbg.graph()
    assert len(nodes) > 0
    assert len(edges) > 0

    # Simplify conflicts
    pbg = pbg.simplify_conflicts(pbg)

    # CompressedProblemsGraph
    cp_pbg = libmambapy.solver.CompressedProblemsGraph.from_problems_graph(pbg)

    assert isinstance(cp_pbg.root_node(), int)
    assert len(cp_pbg.conflicts()) == 2
    nodes, edges = cp_pbg.graph()
    assert len(nodes) > 0
    assert len(edges) > 0
    assert "is not installable" in cp_pbg.tree_message(libmambapy.solver.ProblemsMessageFormat())


def test_CompressedProblemsGraph_NamedList():
    ProblemsGraph = libmambapy.solver.ProblemsGraph
    CompressedProblemsGraph = libmambapy.solver.CompressedProblemsGraph
    PackageInfo = libmambapy.specs.PackageInfo

    named_list = CompressedProblemsGraph.PackageListNode()
    assert len(named_list) == 0
    assert not named_list

    # Add
    for ver, bld in [("1.0", "bld1"), ("2.0", "bld2"), ("3.0", "bld3"), ("4.0", "bld4")]:
        named_list.add(ProblemsGraph.PackageNode(PackageInfo("a", version=ver, build_string=bld)))

    # Enumeration
    assert len(named_list) == 4
    assert named_list
    assert len(list(named_list)) == len(named_list)

    # Methods
    assert named_list.name() == "a"
    list_str, count = named_list.versions_trunc(sep=":", etc="*", threshold=2)
    assert count == 4
    assert list_str == "1.0:2.0:*:4.0"
    list_str, count = named_list.build_strings_trunc(sep=":", etc="*", threshold=2)
    assert count == 4
    assert list_str == "bld1:bld2:*:bld4"
    list_str, count = named_list.versions_and_build_strings_trunc(sep=":", etc="*", threshold=2)
    assert count == 4
    assert list_str == "1.0 bld1:2.0 bld2:*:4.0 bld4"

    # Clear
    named_list.clear()
    assert len(named_list) == 0


def test_RepodataParser():
    assert libmambapy.solver.RepodataParser.Mamba.name == "Mamba"
    assert libmambapy.solver.RepodataParser.Libsolv.name == "Libsolv"

    assert libmambapy.solver.RepodataParser("Libsolv") == libmambapy.solver.RepodataParser.Libsolv

    with pytest.raises(KeyError):
        libmambapy.solver.RepodataParser("NoParser")


def test_PipASPythonDependency():
    assert libmambapy.solver.PipAsPythonDependency.No.name == "No"
    assert libmambapy.solver.PipAsPythonDependency.Yes.name == "Yes"

    assert (
        libmambapy.solver.PipAsPythonDependency(True) == libmambapy.solver.PipAsPythonDependency.Yes
    )


def test_PackageTypes():
    assert libmambapy.solver.PackageTypes.CondaOnly.name == "CondaOnly"
    assert libmambapy.solver.PackageTypes.TarBz2Only.name == "TarBz2Only"
    assert libmambapy.solver.PackageTypes.CondaAndTarBz2.name == "CondaAndTarBz2"
    assert libmambapy.solver.PackageTypes.CondaOrElseTarBz2.name == "CondaOrElseTarBz2"

    assert libmambapy.solver.PackageTypes("TarBz2Only") == libmambapy.solver.PackageTypes.TarBz2Only

    with pytest.raises(KeyError):
        libmambapy.solver.RepodataParser("tarbz2-only")


def test_VerifyPackages():
    assert libmambapy.solver.VerifyPackages.No.name == "No"
    assert libmambapy.solver.VerifyPackages.Yes.name == "Yes"

    assert libmambapy.solver.VerifyPackages(True) == libmambapy.solver.VerifyPackages.Yes


def test_Platform():
    assert libmambapy.solver.LogLevel.Debug.name == "Debug"
    assert libmambapy.solver.LogLevel.Warning.name == "Warning"
    assert libmambapy.solver.LogLevel.Error.name == "Error"
    assert libmambapy.solver.LogLevel.Fatal.name == "Fatal"

    assert libmambapy.solver.LogLevel("Error") == libmambapy.solver.LogLevel.Error

    with pytest.raises(KeyError):
        # No parsing, explicit name
        libmambapy.solver.LogLevel("Unicorn")


def test_Priorities():
    p = libmambapy.solver.Priorities(priority=-1, subpriority=-2)

    assert p.priority == -1
    assert p.subpriority == -2

    # Setters
    p.priority = 33
    p.subpriority = 0
    assert p.priority == 33
    assert p.subpriority == 0

    # Operators
    assert p == p
    assert p != libmambapy.solver.Priorities()

    # Copy
    other = copy.deepcopy(p)
    assert other is not p
    assert other == p


def test_RepodataOrigin():
    orig = libmambapy.solver.RepodataOrigin(
        url="https://conda.anaconda.org/conda-forge", mod="the-mod", etag="the-etag"
    )

    assert orig.url == "https://conda.anaconda.org/conda-forge"
    assert orig.etag == "the-etag"
    assert orig.mod == "the-mod"

    # Setters
    orig.url = "https://repo.mamba.pm"
    orig.etag = "other-etag"
    orig.mod = "other-mod"
    assert orig.url == "https://repo.mamba.pm"
    assert orig.etag == "other-etag"
    assert orig.mod == "other-mod"

    # Operators
    assert orig == orig
    assert orig != libmambapy.solver.RepodataOrigin()

    # Copy
    other = copy.deepcopy(orig)
    assert other is not orig
    assert other == orig
