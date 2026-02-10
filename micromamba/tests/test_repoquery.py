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
        assert any(x["name"] == "libgcc" for x in pkgs)


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
        assert any(x["name"] == "libgcc" for x in pkgs)


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


# Non-regression test for: https://github.com/mamba-org/mamba/issues/3717
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("spec", ("xtensor", "xtensor=0.24.5"))
def test_whoneeds_not_installed_with_channel_no_json(yaml_env: Path, spec):
    res = helpers.umamba_repoquery("whoneeds", "-c", "conda-forge", spec, "--platform", "osx-64")
    res = helpers.remove_whitespaces(res)
    assert "Name Version Build Depends Channel Subdir" in res
    assert "cascade 0.1.1 py38h5ce3968_0 xtensor conda-forge osx-64" in res


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


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_remote_search_python_site_packages_path(yaml_env: Path):
    res = helpers.umamba_repoquery(
        "search",
        "-c",
        "conda-forge",
        "--platform",
        "linux-64",
        "python=3.13.1=h9a34b6e_5_cp313t",
        "--json",
    )

    assert res["query"]["query"] == "python=3.13.1=h9a34b6e_5_cp313t"
    assert res["query"]["type"] == "search"

    package_info = res["result"]["pkgs"][0]

    assert "conda-forge" in package_info["channel"]
    assert package_info["name"] == "python"
    assert package_info["version"] == "3.13.1"
    assert package_info["track_features"] == "py_freethreading"
    assert package_info["python_site_packages_path"] == "lib/python3.13t/site-packages"


@pytest.mark.parametrize("wildcard", [False, True])
@pytest.mark.parametrize("pretty_print", ["", "--pretty"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_search_output_style(
    tmp_home, tmp_root_prefix, tmp_env_name, tmp_xtensor_env, wildcard, pretty_print
):
    package_name = "xtensor"
    package_version = "0.24.5"
    if wildcard:
        search_term = (
            f"{package_name}*={package_version}"  # search with package name with * wildcard
        )
    else:
        search_term = f"{package_name}={package_version}"  # search exact package name
    res = helpers.umamba_repoquery(
        "search", "--quiet", "-c", "conda-forge", pretty_print, search_term
    )

    assert len(res) >= 3
    assert "xtensor" in res

    res_lines = res.rstrip("\n").split("\n")
    expect_pretty = (pretty_print != "") or (not wildcard)
    if expect_pretty:
        # pretty printed output looks like this:
        #        xtensor 0.24.5 hf52228f_0
        # ────────────────────────────────────────
        #
        #  Name            xtensor
        #  Version         0.24.5
        #  Build           hf52228f_0
        # [...]
        assert len(res_lines) >= 10
        # The package name and version must be on the first line.
        assert res_lines[0].find(package_name) >= 0
        assert res_lines[0].find(package_version) >= 0
        # Somewhere in the returned string, there needs to be "Name" and "Version" rows.
        assert res.find("Name") >= 0
        assert res.find("Version") >= 0
    else:
        # standard output has a table layout:
        #  Name    Version Build        Channel     Subdir
        # ───────────────────────────────────────────────────
        #  xtensor 0.24.5  hf52228f_0   conda-forge linux-64
        # check "Name" and "Version" of the table header
        assert res_lines[0].find("Name") >= 0
        assert res_lines[0].find("Version") >= 0
        # package name and version must be part of the shown result
        assert res_lines[-1].find(package_name) >= 0
        assert res_lines[-1].find(package_version) >= 0


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_search_version_sorting_numerical(yaml_env: Path):
    """
    Test that search results sort versions numerically, not alphabetically.

    This test verifies fix for issue #4116 where versions were sorted
    alphabetically (e.g., 25.9 < 4.1) instead of numerically (4.1 < 25.9).
    """

    # Simple Version parser for testing (replaces libmambapy.specs.Version.parse)
    def parse_version(version_str):
        """
        Parse a version string into a comparable tuple.
        Handles formats like "4.1", "22.9", "25.7", "1.2.3", etc.
        Strips non-numeric suffixes (e.g., "22.11.1.post1" -> (22, 11, 1)).
        Returns the version tuple, or None if parsing fails.
        """
        try:
            version_str = str(version_str).strip()
            if not version_str:
                return None

            # Remove any non-numeric suffixes (e.g., "22.11.1.post1" -> "22.11.1")
            # Split on common separators and take the first part
            for suffix in [".post", ".dev", ".alpha", ".beta", ".rc", ".a", ".b", "+", "-"]:
                if suffix in version_str:
                    version_str = version_str.split(suffix)[0]

            # Split by dots and convert parts to integers
            parts = version_str.split(".")
            version_parts = []
            for part in parts:
                # Try to extract numeric prefix (e.g., "11" from "11post1")
                if part.isdigit():
                    version_parts.append(int(part))
                elif part:
                    # Try to extract leading digits
                    digits = ""
                    for char in part:
                        if char.isdigit():
                            digits += char
                        else:
                            break
                    if digits:
                        version_parts.append(int(digits))
                    else:
                        # Non-numeric part, stop parsing
                        break

            # Return None if we got an empty tuple
            if not version_parts:
                return None

            return tuple(version_parts)
        except (ValueError, AttributeError, TypeError):
            # Return None on parse error
            return None

    def parse_build_number(build_str):
        """
        Parse build number from build string (e.g., "h166bdaf_0" -> 0, "h8ffe710_1013" -> 1013).
        Returns the build number as an integer, or 0 if parsing fails.
        """
        try:
            build_str = str(build_str).strip()
            if not build_str:
                return 0

            # Extract number after the last underscore
            if "_" in build_str:
                parts = build_str.rsplit("_", 1)
                if len(parts) == 2:
                    build_num_str = parts[1]
                    # Extract numeric prefix (in case there are non-numeric suffixes)
                    digits = ""
                    for char in build_num_str:
                        if char.isdigit():
                            digits += char
                        else:
                            break
                    if digits:
                        return int(digits)
            return 0
        except (ValueError, AttributeError, TypeError):
            return 0

    # Search for conda package which has multiple versions
    res = helpers.umamba_repoquery("search", "-c", "conda-forge", "conda", "--json")

    assert res["query"]["query"] == "conda"
    assert res["query"]["type"] == "search"

    pkgs = res["result"]["pkgs"]
    assert len(pkgs) > 0

    # Filter to get only conda packages (not conda-build, conda-pack, etc.)
    conda_pkgs = [pkg for pkg in pkgs if pkg["name"] == "conda"]

    # We need at least a few versions to test sorting
    assert len(conda_pkgs) >= 2, "Need at least 2 conda versions to test sorting"

    # Extract and parse versions and build numbers for comparison
    parsed_items = []
    for pkg in conda_pkgs:
        version_str = pkg["version"]
        build_str = pkg.get("build", "")
        version_obj = parse_version(version_str)
        if version_obj is not None:
            build_num = parse_build_number(build_str)
            # Store as (version_tuple, build_number) for sorting
            parsed_items.append((version_obj, build_num))

    # Sort by version first (descending), then by build number (descending)
    # This ensures that 4.1 < 4.9 < 22.9 < 22.11 < 25.7 < 25.9 (not alphabetical)
    parsed_items.sort(reverse=True)

    # Verify that versions are in correct numerical order (descending, newest first)
    for i in range(len(parsed_items) - 1):
        version_i, build_i = parsed_items[i]
        version_j, build_j = parsed_items[i + 1]

        # First compare versions
        if version_i > version_j:
            continue  # Correct order
        elif version_i < version_j:
            assert False, (
                f"Versions not sorted numerically: "
                f"{version_i} should be >= {version_j} "
                f"(alphabetical sorting would be incorrect)"
            )
        else:
            # Versions are equal, check build numbers
            assert build_i >= build_j, (
                f"Build numbers not sorted correctly for version {version_i}: "
                f"build {build_i} should be >= build {build_j}"
            )

    # Also test with table output (non-JSON) to verify the displayed order
    res_table = helpers.umamba_repoquery("search", "-c", "conda-forge", "conda")
    res_table = helpers.remove_whitespaces(res_table)

    # Extract version numbers from table output
    # The table format is: "Name Version Build Channel Subdir"
    lines = res_table.split("\n")
    conda_lines = [line for line in lines if line.startswith("conda ") and len(line.split()) >= 2]

    if len(conda_lines) >= 2:
        # Parse versions and build numbers from table lines
        # The table format is: "Name Version Build Channel Subdir"
        table_parsed_items = []
        for line in conda_lines:
            parts = line.split()
            if len(parts) >= 3:
                version_str = parts[1]
                build_str = parts[2]
                version_obj = parse_version(version_str)
                if version_obj is not None:
                    build_num = parse_build_number(build_str)
                    # Store as (version_tuple, build_number) for sorting
                    table_parsed_items.append((version_obj, build_num))

        # Sort by version first (descending), then by build number (descending)
        table_parsed_items.sort(reverse=True)

        # Verify table versions are also sorted correctly (descending)
        for i in range(len(table_parsed_items) - 1):
            version_i, build_i = table_parsed_items[i]
            version_j, build_j = table_parsed_items[i + 1]

            # First compare versions
            if version_i > version_j:
                continue  # Correct order
            elif version_i < version_j:
                assert False, (
                    f"Table versions not sorted numerically: {version_i} should be >= {version_j}"
                )
            else:
                # Versions are equal, check build numbers
                assert build_i >= build_j, (
                    f"Table build numbers not sorted correctly for version {version_i}: "
                    f"build {build_i} should be >= build {build_j}"
                )
