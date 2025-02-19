import json
import itertools

import pytest

import libmambapy
import libmambapy.solver.libsolv as libsolv


def test_import_submodule():
    import libmambapy.solver.libsolv as libsolv

    # Dummy execution
    _p = libsolv.Solver


def test_import_recursive():
    import libmambapy as mamba

    # Dummy execution
    _p = mamba.solver.libsolv.Solver


def test_Database_logger():
    db = libsolv.Database(libmambapy.specs.ChannelResolveParams())

    def logger(level, msg): ...

    db.set_logger(logger)


@pytest.mark.parametrize("add_pip_as_python_dependency", [True, False])
def test_Database_RepoInfo_from_packages(add_pip_as_python_dependency):
    db = libsolv.Database(libmambapy.specs.ChannelResolveParams())
    assert db.repo_count() == 0
    assert db.installed_repo() is None
    assert db.package_count() == 0

    repo = db.add_repo_from_packages(
        [libmambapy.specs.PackageInfo(name="python")],
        name="duck",
        add_pip_as_python_dependency=add_pip_as_python_dependency,
    )
    db.set_installed_repo(repo)

    assert repo.id > 0
    assert repo.name == "duck"
    assert repo.priority == libsolv.Priorities()
    assert repo.package_count() == 1
    assert db.repo_count() == 1
    assert db.package_count() == 1
    assert db.installed_repo() == repo

    new_priority = libsolv.Priorities(2, 3)
    db.set_repo_priority(repo, new_priority)
    assert repo.priority == new_priority

    pkgs = db.packages_in_repo(repo)
    assert len(pkgs) == 1
    assert pkgs[0].name == "python"
    assert pkgs[0].dependencies == [] if add_pip_as_python_dependency else ["pip"]

    db.remove_repo(repo)
    assert db.repo_count() == 0
    assert db.package_count() == 0
    assert db.installed_repo() is None


@pytest.fixture
def tmp_repodata_json(tmp_path):
    file = tmp_path / "repodata.json"
    with open(file, "w+") as f:
        json.dump(
            {
                "packages": {
                    "python-1.0-bld": {
                        "name": "python",
                        "version": "1.0",
                        "build": "bld",
                        "build_number": 0,
                    },
                },
                "packages.conda": {
                    "foo-1.0-bld": {
                        "name": "foo",
                        "version": "1.0",
                        "build": "bld",
                        "build_number": 0,
                    }
                },
            },
            f,
        )
    return file


@pytest.mark.parametrize(
    ["add_pip_as_python_dependency", "package_types", "repodata_parser"],
    itertools.product(
        [True, False],
        ["TarBz2Only", "CondaOrElseTarBz2"],
        ["Mamba", "Libsolv"],
    ),
)
def test_Database_RepoInfo_from_repodata(
    tmp_path, tmp_repodata_json, add_pip_as_python_dependency, package_types, repodata_parser
):
    db = libsolv.Database(libmambapy.specs.ChannelResolveParams())

    url = "https://repo.mamba.pm"
    channel_id = "conda-forge"

    # Json
    repo = db.add_repo_from_repodata_json(
        path=tmp_repodata_json,
        url=url,
        channel_id=channel_id,
        add_pip_as_python_dependency=add_pip_as_python_dependency,
        package_types=package_types,
        repodata_parser=repodata_parser,
    )
    db.set_installed_repo(repo)

    assert repo.package_count() == 1 if package_types in ["TarBz2Only", "CondaOnly"] else 2
    assert db.package_count() == repo.package_count()

    pkgs = db.packages_in_repo(repo)
    assert len(pkgs) == repo.package_count()
    python_pkg = next(p for p in pkgs if p.name == "python")
    assert python_pkg.dependencies == [] if add_pip_as_python_dependency else ["pip"]

    # Native serialize repo
    solv_file = tmp_path / "repodata.solv"

    origin = libsolv.RepodataOrigin(url=url)
    repo_saved = db.native_serialize_repo(repo, path=solv_file, metadata=origin)
    assert repo_saved == repo

    # Native deserialize repo
    db.remove_repo(repo)
    assert db.package_count() == 0

    repo_loaded = db.add_repo_from_native_serialization(
        path=solv_file,
        expected=origin,
        channel_id=channel_id,
        add_pip_as_python_dependency=add_pip_as_python_dependency,
    )
    assert repo_loaded.package_count() == 1 if package_types in ["TarBz2Only", "CondaOnly"] else 2


def test_Database_RepoInfo_from_repodata_error():
    db = libsolv.Database(libmambapy.specs.ChannelResolveParams())
    channel_id = "conda-forge"

    with pytest.raises(libmambapy.MambaNativeException, match=r"[/\\]does[/\\]not[/\\]exists"):
        db.add_repo_from_repodata_json(
            path="/does/not/exists", url="https://repo..mamba.pm", channel_id=channel_id
        )

    with pytest.raises(libmambapy.MambaNativeException, match=r"[/\\]does[/\\]not[/\\]exists"):
        db.add_repo_from_native_serialization(
            path="/does/not/exists", expected=libsolv.RepodataOrigin(), channel_id=channel_id
        )


def test_Solver_UnSolvable():
    Request = libmambapy.solver.Request

    db = libsolv.Database(libmambapy.specs.ChannelResolveParams())

    request = Request([Request.Install(libmambapy.specs.MatchSpec.parse("a>1.0"))])

    solver = libsolv.Solver()
    outcome = solver.solve(db, request)

    assert isinstance(outcome, libsolv.UnSolvable)
    assert len(outcome.problems(db)) > 0
    assert isinstance(outcome.problems_to_str(db), str)
    assert isinstance(outcome.all_problems_to_str(db), str)
    assert "The following package could not be installed" in outcome.explain_problems(
        db, libmambapy.solver.ProblemsMessageFormat()
    )
    assert outcome.problems_graph(db).graph() is not None


def test_Solver_Solution():
    Request = libmambapy.solver.Request

    db = libsolv.Database(libmambapy.specs.ChannelResolveParams())
    db.add_repo_from_packages(
        [libmambapy.specs.PackageInfo(name="foo")],
    )

    request = Request([Request.Install(libmambapy.specs.MatchSpec.parse("foo"))])

    solver = libsolv.Solver()
    outcome = solver.solve(db, request)

    assert isinstance(outcome, libmambapy.solver.Solution)
    assert len(outcome.actions) == 1
