import platform
from pathlib import Path

import pytest

from . import helpers


@pytest.fixture
def yaml_env(tmp_prefix: Path) -> None:
    helpers.install(
        "--channel",
        "conda-forge",
        "yaml=0.2.5",
        "pyyaml=6.0.0",
        no_dry_run=True,
    )


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_depends_local(yaml_env: Path):
    """Depends with local repository."""
    res = helpers.umamba_repoquery("depends", "yaml", "--json")

    assert res["query"]["query"] == "yaml"
    assert res["query"]["type"] == "depends"

    pkgs = res["result"]["pkgs"]
    assert any(x["name"] == "yaml" for x in pkgs)
    assert any(x["version"] == "0.2.5" for x in pkgs)

    if platform.system() == "Linux":
        assert any(x["name"] == "libgcc-ng" for x in pkgs)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_depends_local_not_installed(yaml_env: Path):
    res = helpers.umamba_repoquery("depends", "xtensor")

    assert 'No entries matching "xtensor" found' in res
    assert "Try looking remotely with '--remote'." in res


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_depends_remote(yaml_env: Path):
    res = helpers.umamba_repoquery("depends", "yaml=0.2.5", "--remote", "--json")

    assert res["query"]["query"] == "yaml=0.2.5"
    assert res["query"]["type"] == "depends"

    pkgs = res["result"]["pkgs"]
    assert any(x["name"] == "yaml" for x in pkgs)
    assert any(x["version"] == "0.2.5" for x in pkgs)

    if platform.system() == "Linux":
        assert any(x["name"] == "libgcc-ng" for x in pkgs)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("with_platform", (False, True))
def test_depends_not_installed_with_channel(yaml_env: Path, with_platform):
    if with_platform:
        res = helpers.umamba_repoquery(
            "depends",
            "-c",
            "conda-forge",
            "xtensor=0.24.5",
            "--platform",
            "win-64",
            "--json",
        )
        assert res["result"]["pkgs"][0]["subdir"] == "win-64"
    else:
        res = helpers.umamba_repoquery("depends", "-c", "conda-forge", "xtensor=0.24.5", "--json")

    assert res["query"]["query"] == "xtensor=0.24.5"
    assert res["query"]["type"] == "depends"
    assert "conda-forge" in res["result"]["graph_roots"][0]["channel"]
    assert res["result"]["graph_roots"][0]["name"] == "xtensor"
    assert res["result"]["graph_roots"][0]["version"] == "0.24.5"

    pkgs = res["result"]["pkgs"]

    assert any(x["name"] == "xtensor" for x in pkgs)
    assert any(x["name"] == "xtl" for x in pkgs)

    if not with_platform and platform.system() == "Linux":
        assert any(x["name"] == "libgcc-ng" for x in pkgs)
        assert any(x["name"] == "libstdcxx-ng" for x in pkgs)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_depends_recursive(yaml_env: Path):
    res = helpers.umamba_repoquery("depends", "-c", "conda-forge", "xtensor=0.24.5", "--recursive")

    if platform.system() == "Linux":
        assert "_openmp_mutex" in res
    elif platform.system() == "Darwin":
        assert "libcxx" in res
    elif platform.system() == "Windows":
        assert "vc" in res


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_depends_tree(yaml_env: Path):
    res = helpers.umamba_repoquery("depends", "-c", "conda-forge", "xtensor=0.24.5", "--tree")

    if platform.system() == "Linux":
        assert "_openmp_mutex" in res
    elif platform.system() == "Darwin":
        assert "libcxx" in res
    elif platform.system() == "Windows":
        assert "vc" in res


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_whoneeds_local(yaml_env: Path):
    res = helpers.umamba_repoquery("whoneeds", "yaml", "--json")

    assert res["query"]["query"] == "yaml"
    assert res["query"]["type"] == "whoneeds"
    assert res["result"]["pkgs"][0]["channel"] == "conda-forge"
    assert res["result"]["pkgs"][0]["name"] == "pyyaml"
    assert res["result"]["pkgs"][0]["version"] == "6.0"


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_whoneeds_local_not_installed(yaml_env: Path):
    res = helpers.umamba_repoquery("whoneeds", "xtl")

    assert 'No entries matching "xtl" found' in res
    assert "Try looking remotely with '--remote'." in res


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_whoneeds_remote(yaml_env: Path):
    res = helpers.umamba_repoquery("whoneeds", "xtl=0.7.7", "--remote", "--json")

    # TODO: check why
    if platform.machine() != "arm64":
        assert "xproperty" in {pkg["name"] for pkg in res["result"]["pkgs"]}


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("with_platform", (False, True))
def test_whoneeds_not_installed_with_channel(yaml_env: Path, with_platform):
    if with_platform:
        res = helpers.umamba_repoquery(
            "whoneeds", "-c", "conda-forge", "xtensor=0.24.5", "--platform", "osx-64", "--json"
        )
        assert all("osx-64" in pkg["subdir"] for pkg in res["result"]["pkgs"])
    else:
        res = helpers.umamba_repoquery("whoneeds", "-c", "conda-forge", "xtensor=0.24.5", "--json")

    assert res["query"]["query"] == "xtensor=0.24.5"
    assert res["query"]["type"] == "whoneeds"

    pkgs = res["result"]["pkgs"]
    assert all("conda-forge" in x["channel"] for x in pkgs)
    assert any(x["name"] == "cppcolormap" for x in pkgs)
    assert any(x["name"] == "pyxtensor" for x in pkgs)
    assert any(x["name"] == "qpot" for x in pkgs)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("spec", ("xtensor", "xtensor=0.24.5"))
def test_whoneeds_not_installed_with_channel_no_json(yaml_env: Path, spec):
    res = helpers.umamba_repoquery("whoneeds", "-c", "conda-forge", spec, "--platform", "osx-64")
    assert "Name Version Build Depends Channel Subdir" in helpers.remove_whitespaces(res)
    if spec == "xtensor=0.24.5":  # `Depends` column is empty here (bug?)
        assert "cascade 0.1.1 py38h5ce3968_0 conda-forge osx-64" in helpers.remove_whitespaces(res)
    else:
        assert (
            "cascade 0.1.1 py38h5ce3968_0 xtensor conda-forge osx-64"
            in helpers.remove_whitespaces(res)
        )


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_whoneeds_tree(yaml_env: Path):
    res = helpers.umamba_repoquery("whoneeds", "-c", "conda-forge", "xtensor=0.24.5", "--tree")

    assert "cppcolormap" in res
    assert "pyxtensor" in res
    assert "qpot" in res


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_search_local_not_installed(yaml_env: Path):
    res = helpers.umamba_repoquery("search", "xtensor", "--local")

    assert 'No entries matching "xtensor" found' in res


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_search_local_installed_pkg(yaml_env: Path):
    res = helpers.umamba_repoquery("search", "yaml", "--local", "--json")

    assert res["query"]["query"] == "yaml"
    assert res["query"]["type"] == "search"
    assert res["result"]["pkgs"][0]["channel"] == "conda-forge"
    assert res["result"]["pkgs"][0]["name"] == "yaml"
    assert res["result"]["pkgs"][0]["version"] == "0.2.5"


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("with_platform", (False, True))
def test_search_remote(yaml_env: Path, with_platform):
    if with_platform:
        res = helpers.umamba_repoquery(
            "search",
            "-c",
            "conda-forge",
            "xtensor*",
            "--platform",
            "linux-64",
            "--json",
        )
        assert res["result"]["pkgs"][0]["subdir"] == "linux-64"
    else:
        res = helpers.umamba_repoquery("search", "-c", "conda-forge", "xtensor*", "--json")

    assert res["query"]["query"] == "xtensor*"
    assert res["query"]["type"] == "search"

    pkgs = res["result"]["pkgs"]
    assert all("conda-forge" in x["channel"] for x in pkgs)
    assert any(x["name"] == "xtensor-blas" for x in pkgs)
    assert any(x["name"] == "xtensor" for x in pkgs)

    # xtensor-io is not available yet on osx-arm64
    if platform.machine() != "arm64":
        assert any(x["name"] == "xtensor-io" for x in pkgs)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_remote_search_not_installed_pkg(yaml_env: Path):
    res = helpers.umamba_repoquery("search", "-c", "conda-forge", "xtensor=0.24.5", "--json")

    assert res["query"]["query"] == "xtensor=0.24.5"
    assert res["query"]["type"] == "search"
    assert "conda-forge" in res["result"]["pkgs"][0]["channel"]
    assert res["result"]["pkgs"][0]["name"] == "xtensor"
    assert res["result"]["pkgs"][0]["version"] == "0.24.5"
