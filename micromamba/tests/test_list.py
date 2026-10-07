import platform
import subprocess
import sys

import pytest
import re

from . import helpers


@pytest.mark.parametrize("reverse_flag", ["", "--reverse"])
@pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
@pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list(
    tmp_home, tmp_root_prefix, tmp_env_name, tmp_xtensor_env, env_selector, quiet_flag, reverse_flag
):
    if env_selector == "prefix":
        res = helpers.umamba_list("-p", tmp_xtensor_env, "--json", quiet_flag, reverse_flag)
    elif env_selector == "name":
        res = helpers.umamba_list("-n", tmp_env_name, "--json", quiet_flag, reverse_flag)
    else:
        res = helpers.umamba_list("--json", quiet_flag, reverse_flag)

    assert len(res) > 2

    names = [i["name"] for i in res]
    assert "xtensor" in names
    assert "xtl" in names
    assert all(
        i["channel"] == "conda-forge"
        and i["base_url"] == "https://conda.anaconda.org/conda-forge"
        and i["name"] in i["url"]
        and "conda-forge" in i["url"]
        for i in res
    )

    if reverse_flag == "--reverse":
        assert names.index("xtensor") > names.index("xtl")
    else:
        assert names.index("xtensor") < names.index("xtl")


@pytest.mark.parametrize("reverse_flag", ["", "--reverse"])
@pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
@pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list_no_json(
    tmp_home, tmp_root_prefix, tmp_env_name, tmp_xtensor_env, env_selector, quiet_flag, reverse_flag
):
    if env_selector == "prefix":
        res = helpers.umamba_list("-p", tmp_xtensor_env, quiet_flag, reverse_flag)
    elif env_selector == "name":
        res = helpers.umamba_list("-n", tmp_env_name, quiet_flag, reverse_flag)
    else:
        res = helpers.umamba_list(quiet_flag, reverse_flag)

    assert len(res) > 10

    assert "xtensor" in res
    assert "xtl" in res

    # This is what res looks like in this case:
    # List of packages in environment: "xxx"

    # Name           Version  Build        Channel
    # ────────────────────────────────────────────────────
    # _libgcc_mutex  0.1      conda_forge  conda-forge
    # _openmp_mutex  4.5      2_gnu        conda-forge
    packages = res[res.rindex("Channel") :].split("\n", 1)[1]
    packages_list = packages.strip().split("\n")[1:]
    for package in packages_list:
        channel = package.split(" ")[-1]
        channel = channel.replace("\r", "")
        assert channel == "conda-forge"

    if reverse_flag == "--reverse":
        assert res.find("xtensor") > res.find("xtl")
    else:
        assert res.find("xtensor") < res.find("xtl")


@pytest.mark.parametrize("explicit_flag", ["", "--explicit"])
@pytest.mark.parametrize("md5_flag", ["", "--md5"])
@pytest.mark.parametrize("sha256_flag", ["", "--sha256"])
@pytest.mark.parametrize("canonical_flag", ["", "-c", "--canonical"])
@pytest.mark.parametrize("export_flag", ["", "-e", "--export"])
@pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list_subcommands(
    tmp_home,
    tmp_root_prefix,
    tmp_env_name,
    tmp_xtensor_env,
    env_selector,
    explicit_flag,
    md5_flag,
    sha256_flag,
    canonical_flag,
    export_flag,
):
    args = []
    if env_selector == "prefix":
        args += ["-p", tmp_xtensor_env]
    elif env_selector == "name":
        args += ["-n", tmp_env_name]
    args += [explicit_flag, md5_flag, sha256_flag, canonical_flag, export_flag]

    if (explicit_flag == "--explicit") and (md5_flag == "--md5") and (sha256_flag == "--sha256"):
        with pytest.raises(subprocess.CalledProcessError) as excinfo:
            helpers.umamba_list(*args)
            assert "Only one of --md5 and --sha256 can be specified at the same time." in excinfo
        return None

    res = helpers.umamba_list(*args)

    outputs_list = res.strip().split("\n")[2:]
    outputs_list = [i for i in outputs_list if i != "" and not i.startswith("Warning")]
    items = ["conda-forge/", "::"]
    if explicit_flag == "--explicit":
        for output in outputs_list:
            assert "/conda-forge/" in output
            if (md5_flag == "--md5") or (sha256_flag == "--sha256"):
                assert "#" in output
                hash = output.split("#")[-1]
                hash = hash.replace("\r", "")
                if md5_flag == "--md5":
                    assert len(hash) == 32
                else:
                    assert len(hash) == 64
            else:
                assert "#" not in output
    elif canonical_flag in ["-c", "--canonical"]:
        for output in outputs_list:
            assert all(i in output for i in items)
            assert " " not in output
    elif export_flag in ["-e", "--export"]:
        items += [" "]
        for output in outputs_list:
            assert all(i not in output for i in items)
            assert len(output.split("=")) == 3


def _inject_token_into_conda_meta(prefix, token):
    import json
    import pathlib

    conda_meta = pathlib.Path(prefix) / "conda-meta"
    count = 0
    for record in conda_meta.glob("*.json"):
        data = json.loads(record.read_text())
        url = data.get("url", "")
        if "://" not in url or "/t/" in url:
            continue
        scheme, rest = url.split("://", 1)
        host, path = rest.split("/", 1)
        data["url"] = f"{scheme}://{host}/t/{token}/{path}"
        record.write_text(json.dumps(data))
        count += 1
    return count


@pytest.mark.parametrize("hash_flag", ["", "--md5", "--sha256"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list_explicit_auth(tmp_home, tmp_root_prefix, tmp_xtensor_env, hash_flag):
    token = "sometoken123abc"
    assert _inject_token_into_conda_meta(tmp_xtensor_env, token) > 0
    token_fragment = f"/t/{token}/"

    # Default: Authentication stripped from URLs.
    default_res = helpers.umamba_list("-p", tmp_xtensor_env, "--explicit", hash_flag)
    default_urls = [line for line in default_res.splitlines() if "://" in line]
    assert len(default_urls) > 0
    for line in default_urls:
        assert token_fragment not in line
        assert line.startswith("https://")
        if hash_flag == "--md5":
            assert len(line.split("#")[-1].strip()) == 32
        elif hash_flag == "--sha256":
            assert len(line.split("#")[-1].strip()) == 64

    # With --auth: Authentication preserved in URLs.
    auth_res = helpers.umamba_list("-p", tmp_xtensor_env, "--explicit", "--auth", hash_flag)
    auth_urls = [line for line in auth_res.splitlines() if "://" in line]
    assert len(auth_urls) > 0
    for line in auth_urls:
        assert token_fragment in line


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list_auth_without_explicit_is_ignored(tmp_home, tmp_root_prefix, tmp_xtensor_env):
    res = helpers.umamba_list("-p", tmp_xtensor_env, "--auth")
    assert "xtensor" in res
    assert "xtl" in res


@pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list_name(tmp_home, tmp_root_prefix, tmp_xtensor_env, quiet_flag):
    helpers.install("xtensor-python")
    res = helpers.umamba_list("xt", "--json", quiet_flag)
    names = sorted([i["name"] for i in res])
    assert names == ["xtensor", "xtensor-python", "xtl"]

    full_res = helpers.umamba_list("xtensor", "--full-name", "--json", quiet_flag)
    full_names = sorted([i["name"] for i in full_res])
    assert full_names == ["xtensor"]


env_yaml_content_to_install_numpy_with_pip = """
channels:
- conda-forge
dependencies:
- pip
- pip:
  - pandas==2.2.3
"""


@pytest.mark.parametrize("no_pip_flag", ["", "--no-pip"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list_with_pip(tmp_home, tmp_root_prefix, tmp_path, no_pip_flag):
    env_name = "env-list_with_pip"
    tmp_root_prefix / "envs" / env_name

    env_file_yml = tmp_path / "test_env_yaml_content_to_install_numpy_with_pip.yaml"
    env_file_yml.write_text(env_yaml_content_to_install_numpy_with_pip)

    helpers.create("-n", env_name, "python=3.12", "--json", no_dry_run=True)
    helpers.install("-n", env_name, "-f", env_file_yml, "--json", no_dry_run=True)

    res = helpers.umamba_list("-n", env_name, "--json", no_pip_flag)
    if no_pip_flag == "":
        assert any(
            package["name"] == "pandas"
            and package["version"] == "2.2.3"
            and package["base_url"] == "https://pypi.org/"
            and package["build_string"] == "pypi_0"
            and package["channel"] == "pypi"
            and package["platform"] == sys.platform + "-" + platform.machine()
            for package in res
        )
        # Check that dependencies are listed
        assert any(
            package["name"] == "numpy"
            and package["base_url"] == "https://pypi.org/"
            and package["build_string"] == "pypi_0"
            and package["channel"] == "pypi"
            and package["platform"] == sys.platform + "-" + platform.machine()
            for package in res
        )
    else:  # --no-pip
        # Check that pandas installed with pip is not listed
        assert all(package["name"] != "pandas" for package in res)
        # Check that dependencies are not there either
        assert all(package["name"] != "numpy" for package in res)


env_yaml_content_to_install_numpy_with_uv = """
channels:
- conda-forge
dependencies:
- uv
- pip:
  - pandas==2.2.3
"""


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list_with_uv(tmp_home, tmp_root_prefix, tmp_path):
    env_name = "env-list_with_uv"
    tmp_root_prefix / "envs" / env_name

    env_file_yml = tmp_path / "test_env_yaml_content_to_install_numpy_with_uv.yaml"
    env_file_yml.write_text(env_yaml_content_to_install_numpy_with_uv)

    helpers.create("-n", env_name, "python=3.12", "--json", no_dry_run=True)
    helpers.install("-n", env_name, "-f", env_file_yml, "--json", no_dry_run=True)

    res = helpers.umamba_list("-n", env_name, "--json")
    assert any(
        package["name"] == "pandas"
        and package["version"] == "2.2.3"
        and package["base_url"] == "https://pypi.org/"
        and package["build_string"] == "pypi_0"
        and package["channel"] == "pypi"
        and package["platform"] == sys.platform + "-" + platform.machine()
        for package in res
    )
    # Check that dependencies are listed
    assert any(
        package["name"] == "numpy"
        and package["base_url"] == "https://pypi.org/"
        and package["build_string"] == "pypi_0"
        and package["channel"] == "pypi"
        and package["platform"] == sys.platform + "-" + platform.machine()
        for package in res
    )


@pytest.mark.parametrize("env_selector", ["name", "prefix"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_not_existing(tmp_home, tmp_root_prefix, tmp_xtensor_env, env_selector):
    if env_selector == "prefix":
        cmd = ("-p", tmp_root_prefix / "envs" / "does-not-exist", "--json")
    elif env_selector == "name":
        cmd = ("-n", "does-not-exist", "--json")

    with pytest.raises(subprocess.CalledProcessError):
        helpers.umamba_list(*cmd)


def test_not_environment(tmp_home, tmp_root_prefix):
    with pytest.raises(subprocess.CalledProcessError):
        helpers.umamba_list("-p", tmp_root_prefix / "envs", "--json")


@pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_regex(tmp_home, tmp_root_prefix, tmp_xtensor_env, quiet_flag):
    full_res = helpers.umamba_list("--json")
    names = sorted([i["name"] for i in full_res])

    filtered_res = helpers.umamba_list("\\**", "--json", quiet_flag)
    filtered_names = sorted([i["name"] for i in filtered_res])
    assert filtered_names == names

    filtered_res = helpers.umamba_list("^xt", "--json", quiet_flag)
    filtered_names = sorted([i["name"] for i in filtered_res])
    assert filtered_names == ["xtensor", "xtl"]


@pytest.mark.parametrize("revisions_flag", ["", "--revisions"])
@pytest.mark.parametrize("json_flag", ["", "--json"])
def test_revisions(revisions_flag, json_flag):
    env_name = "myenv"

    helpers.create("-n", env_name, "python=3.8")
    helpers.install("-n", env_name, "xeus=2.0")
    helpers.update("-n", env_name, "xeus=4.0")
    helpers.uninstall("-n", env_name, "xeus")
    res = helpers.umamba_list("-n", env_name, revisions_flag, json_flag)

    if revisions_flag == "--revisions":
        if json_flag == "--json":
            # print(res)
            assert all(res[i]["rev"] == i for i in range(len(res)))
            assert any("python-3.8" in i for i in res[0]["install"])
            assert any("xeus-2.0" in i for i in res[2]["remove"])
            assert any("xeus-4.0" in i for i in res[2]["install"])
            assert any("xeus-4.0" in i for i in res[3]["remove"])
            assert len(res[3]["install"]) == 0
        else:
            # Splitting on dates (e.g. 2025-02-18) which are at the beginning of each new revision
            revisions = re.split(r"\d{4}-\d{2}-\d{2}", res)[1:]
            assert all("rev" in revisions[i] for i in range(len(revisions)))
            assert "python-3.8" in revisions[0]
            assert revisions[0].count("+") == len(revisions[0].strip().split("\n")) - 1
            rev_2 = revisions[2].split("\n")[1:]
            assert "xeus-2.0" in revisions[2]
            assert "xeus-4.0" in revisions[2]
            for line in rev_2:
                if "xeus-2.0" in line:
                    assert line.startswith("-")
                elif "xeus-4.0" in line:
                    assert line.startswith("+")
            assert "xeus-4.0" in revisions[3]
            assert "+" not in revisions[3]
    else:
        assert "xeus" not in res
