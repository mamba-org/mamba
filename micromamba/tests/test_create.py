import os
import platform
import shutil
import subprocess
from pathlib import Path

import pytest
import yaml

from . import helpers

__this_dir__ = Path(__file__).parent.resolve()

env_file_requires_pip_install_path = __this_dir__ / "env-requires-pip-install.yaml"


env_file_requires_pip_install_path_with_whitespaces = (
    __this_dir__ / "env-requires-pip-install-with-spaces.yaml"
)

env_files = [
    env_file_requires_pip_install_path,
    env_file_requires_pip_install_path_with_whitespaces,
]

lockfile_path: Path = __this_dir__ / "test_env-lock.yaml"


def check_create_result(res, root_prefix, target_prefix):
    assert res["root_prefix"] == str(root_prefix)
    assert res["target_prefix"] == str(target_prefix)
    assert not res["use_target_prefix_fallback"]
    checks = (
        helpers.MAMBA_ALLOW_EXISTING_PREFIX
        | helpers.MAMBA_NOT_ALLOW_MISSING_PREFIX
        | helpers.MAMBA_ALLOW_NOT_ENV_PREFIX
        | helpers.MAMBA_NOT_EXPECT_EXISTING_PREFIX
    )
    assert res["target_prefix_checks"] == checks


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize(
    "source,file_type",
    [
        ("cli_only", None),
        ("spec_file_only", "classic"),
        ("spec_file_only", "explicit"),
        ("spec_file_only", "yaml"),
        ("both", "classic"),
        ("both", "explicit"),
        ("both", "yaml"),
    ],
)
@pytest.mark.parametrize("create_cmd", ["create", "env create"])
def test_specs(tmp_home, tmp_root_prefix, tmp_path, source, file_type, create_cmd):
    env_prefix = tmp_path / "myenv"

    cmd = ["-p", env_prefix]
    specs = []

    if source in ("cli_only", "both"):
        specs = ["xtensor-python", "xtl"]
        cmd += specs

    if source in ("spec_file_only", "both"):
        spec_file = str(tmp_path / "env")

        if file_type == "classic":
            file_content = ["xtensor >0.20", "xsimd"]
            specs += file_content
        elif file_type == "explicit":
            explicit_specs = [
                "https://conda.anaconda.org/conda-forge/linux-64/xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
                "https://conda.anaconda.org/conda-forge/linux-64/xsimd-7.4.8-hc9558a2_0.tar.bz2#32d5b7ad7d6511f1faacf87e53a63e5f",
            ]
            file_content = ["@EXPLICIT"] + explicit_specs
            specs = explicit_specs
        elif file_type == "yaml":
            spec_file += ".yaml"
            file_content = ["dependencies:", "  - xtensor >0.20", "  - xsimd"]
            specs += ["xtensor >0.20", "xsimd"]
        else:
            raise RuntimeError("unhandled file type : ", file_type)

        with open(spec_file, "w") as f:
            f.write("\n".join(file_content))

        cmd += ["-f", spec_file]

    res = helpers.create(*cmd, "--print-config-only", create_cmd=create_cmd)

    check_create_result(res, tmp_root_prefix, env_prefix)
    assert res["env_name"] == ""
    assert res["specs"] == specs

    json_res = helpers.create(*cmd, "--json", create_cmd=create_cmd)
    assert json_res["success"]


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_lockfile(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "myenv"
    spec_file = tmp_path / "env-lock.yaml"

    shutil.copyfile(lockfile_path, spec_file)

    res = helpers.create("-p", env_prefix, "-f", spec_file, "--json")
    assert res["success"]

    packages = helpers.umamba_list("-p", env_prefix, "--json")
    assert any(package["name"] == "zlib" and package["version"] == "1.2.11" for package in packages)


# TODO: uncomment when https://github.com/mamba-org/mamba/pull/3286 is merged
# @pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
# def test_lockfile_online(tmp_home, tmp_root_prefix, tmp_path):
#    env_prefix = tmp_path / "myenv"
#    spec_file = (
#        "https://raw.githubusercontent.com/mamba-org/mamba/main/micromamba/tests/test_env-lock.yaml"
#    )
#
#    res = helpers.create("-p", env_prefix, "-f", spec_file, "--json")
#    assert res["success"]
#
#    packages = helpers.umamba_list("-p", env_prefix, "--json")
#    assert any(package["name"] == "zlib" and package["version"] == "1.2.11" for package in packages)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_env_lockfile_different_install_after_create(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "myenv"
    create_spec_file = tmp_path / "env-create-lock.yaml"
    install_spec_file = tmp_path / "env-install-lock.yaml"

    shutil.copyfile(__this_dir__ / "envlockfile-check-step-1-lock.yaml", create_spec_file)
    shutil.copyfile(__this_dir__ / "envlockfile-check-step-2-lock.yaml", install_spec_file)

    res = helpers.create("-p", env_prefix, "-f", create_spec_file, "-y", "--json")
    assert res["success"]

    # Must not crash
    helpers.install("-p", env_prefix, "-f", install_spec_file, "-y", "--json")


# Only run this test on Linux, as it is the only platform where xeus-cling
# (which is part of the environment) is available.
@pytest.mark.timeout(30)
@pytest.mark.skipif(platform.system() != "Linux", reason="Test only available on Linux")
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_env_logging_overhead_regression(tmp_home, tmp_root_prefix, tmp_path):
    # Non-regression test https://github.com/mamba-org/mamba/issues/3415.

    env_prefix = tmp_path / "myenv"
    create_spec_file = tmp_path / "env-logging-overhead-regression.yaml"

    shutil.copyfile(__this_dir__ / "env-logging-overhead-regression.yaml", create_spec_file)

    # Must not hang
    helpers.create("-p", env_prefix, "-f", create_spec_file, "-y", "--json", "--dry-run")


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("root_prefix_type", (None, "env_var", "cli"))
@pytest.mark.parametrize("target_is_root", (False, True))
@pytest.mark.parametrize("cli_prefix", (False, True))
@pytest.mark.parametrize("cli_env_name", (False, True))
@pytest.mark.parametrize("yaml_name", (False, True, "prefix"))
@pytest.mark.parametrize("env_var", (False, True))
@pytest.mark.parametrize("fallback", (False, True))
@pytest.mark.parametrize(
    "similar_non_canonical,non_canonical_position",
    ((False, None), (True, "append"), (True, "prepend")),
)
def test_target_prefix(
    tmp_home,
    tmp_root_prefix,
    tmp_path,
    root_prefix_type,
    target_is_root,
    cli_prefix,
    cli_env_name,
    yaml_name,
    env_var,
    fallback,
    similar_non_canonical,
    non_canonical_position,
):
    cmd = []

    if root_prefix_type is None:
        root_prefix = Path(os.environ["MAMBA_ROOT_PREFIX"])
    elif root_prefix_type == "cli":
        root_prefix = tmp_path / "myroot"
        cmd += ["-r", root_prefix]
    else:
        root_prefix = Path(os.environ["MAMBA_ROOT_PREFIX"])

    env_prefix = tmp_path / "myenv"

    if target_is_root:
        p = root_prefix
        n = "base"
    else:
        p = env_prefix
        n = "someenv"

    expected_p = p.resolve()
    if cli_env_name and not target_is_root:
        expected_p = root_prefix / "envs" / n

    if similar_non_canonical:
        if non_canonical_position == "append":
            p = p / "."
        else:
            p = p.parent / "." / p.name

    if cli_prefix:
        cmd += ["-p", p]

    if cli_env_name:
        cmd += ["-n", n]

    if yaml_name:
        spec_file = tmp_path / "env.yaml"

        if yaml_name == "prefix":
            yaml_n = str(p)
        else:
            yaml_n = "yaml_name"
            if not (cli_prefix or cli_env_name):
                expected_p = root_prefix / "envs" / yaml_n

        file_content = [
            f"name: {yaml_n}",
            "dependencies: [xtensor]",
        ]
        with open(spec_file, "w") as f:
            f.write("\n".join(file_content))
        cmd += ["-f", spec_file]

    if env_var:
        os.environ["MAMBA_TARGET_PREFIX"] = str(p)

    if not fallback:
        os.environ.pop("CONDA_PREFIX", None)
    else:
        os.environ["CONDA_PREFIX"] = str(p)

    if (
        (cli_prefix and cli_env_name)
        or (yaml_name == "prefix")
        or not (cli_prefix or cli_env_name or yaml_name or env_var)
    ):
        with pytest.raises(subprocess.CalledProcessError):
            helpers.create(*cmd, "--print-config-only")
    else:
        res = helpers.create(*cmd, "--print-config-only")
        check_create_result(res, root_prefix=root_prefix, target_prefix=expected_p)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("cli", (False, True))
@pytest.mark.parametrize("yaml", (False, True))
@pytest.mark.parametrize("env_var", (False, True))
@pytest.mark.parametrize("rc_file", (False, True))
def test_channels(tmp_home, tmp_root_prefix, tmp_path, cli, yaml, env_var, rc_file):
    env_prefix = tmp_path / "myenv"
    spec_file = tmp_path / "env.yaml"
    rc_file = tmp_path / "rc.yaml"

    cmd = ["-p", env_prefix]
    expected_channels = []

    if cli:
        cmd += ["-c", "cli"]
        expected_channels += ["cli"]

    if yaml:
        file_content = [
            "channels: [yaml]",
            "dependencies: [xtensor]",
        ]

        with open(spec_file, "w") as f:
            f.write("\n".join(file_content))
        cmd += ["-f", spec_file]
        expected_channels += ["yaml"]

    if env_var:
        os.environ["CONDA_CHANNELS"] = "env_var"
        expected_channels += ["env_var"]

    if rc_file:
        file_content = ["channels: [rc]"]
        with open(rc_file, "w") as f:
            f.write("\n".join(file_content))

        cmd += ["--rc-file", rc_file]
        expected_channels += ["rc"]

    res = helpers.create(*cmd, "--print-config-only", no_rc=not rc_file, default_channel=False)
    check_create_result(res, tmp_root_prefix, env_prefix)
    if expected_channels:
        assert res["channels"] == expected_channels
    else:
        assert res["channels"] == ["conda-forge"]


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("type", ("yaml", "classic", "explicit"))
def test_multiple_spec_files(tmp_home, tmp_root_prefix, tmp_path, type):
    env_prefix = tmp_path / "myenv"

    cmd = ["-p", env_prefix]
    specs = ["xtensor", "xsimd"]
    explicit_specs = [
        "https://conda.anaconda.org/conda-forge/linux-64/xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
        "https://conda.anaconda.org/conda-forge/linux-64/xsimd-7.4.8-hc9558a2_0.tar.bz2#32d5b7ad7d6511f1faacf87e53a63e5f",
    ]

    for i in range(2):
        if type == "yaml":
            spec_file = tmp_path / f"env{i}.yaml"
            file_content = [f"dependencies: [{specs[i]}]"]
        elif type == "classic":
            spec_file = tmp_path / f"env{i}.txt"
            file_content = [specs[i]]
        else:  # explicit
            spec_file = tmp_path / f"env{i}.txt"
            file_content = ["@EXPLICIT", explicit_specs[i]]

        with open(spec_file, "w") as f:
            f.write("\n".join(file_content))

        cmd += ["-f", spec_file]

    res = helpers.create(*cmd, "--print-config-only")
    if type == "yaml" or type == "classic":
        assert res["specs"] == specs
    else:  # explicit
        assert res["specs"] == [explicit_specs[0]]


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_multiple_spec_files_different_types(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "myenv"

    cmd = ["-p", env_prefix]

    spec_file_1 = tmp_path / "env1.yaml"
    spec_file_1.write_text("dependencies: [xtensor]")

    spec_file_2 = tmp_path / "env2.txt"
    spec_file_2.write_text("xsimd")

    cmd += ["-f", spec_file_1, "-f", spec_file_2]

    with pytest.raises(subprocess.CalledProcessError) as info:
        helpers.create(*cmd, "--print-config-only")
    assert "found multiple spec file types" in info.value.stderr.decode()


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_multiple_yaml_specs_only_one_has_channels(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "myenv"

    cmd = ["-p", env_prefix]

    spec_file_1 = tmp_path / "env1.yaml"
    spec_file_1.write_text("dependencies: [xtensor]")

    spec_file_2 = tmp_path / "env2.yaml"
    spec_file_2.write_text(
        "dependencies: [xsimd]\nchannels: [bioconda]",
    )

    cmd += ["-f", spec_file_1, "-f", spec_file_2]

    res = helpers.create(*cmd, "--print-config-only", default_channel=False)
    assert res["channels"] == ["bioconda"]
    assert res["specs"] == ["xtensor", "xsimd"]


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_multiple_yaml_specs_different_names(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "myenv"

    cmd = ["-p", env_prefix]

    spec_file_1 = tmp_path / "env1.yaml"
    spec_file_1.write_text("name: env1\ndependencies: [xtensor]")

    spec_file_2 = tmp_path / "env2.yaml"
    spec_file_2.write_text(
        "name: env2\ndependencies: [xsimd]\nchannels: [bioconda]",
    )

    cmd += ["-f", spec_file_1, "-f", spec_file_2]

    res = helpers.create(*cmd, "--print-config-only", default_channel=False)
    assert res["spec_file_env_name"] == "env1"
    assert res["channels"] == ["bioconda"]
    assert res["specs"] == ["xtensor", "xsimd"]


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_multiprocessing(tmp_home, tmp_root_prefix):
    if platform.system() == "Windows":
        return

    root_prefix = Path(os.environ["MAMBA_ROOT_PREFIX"])
    if os.path.exists(root_prefix / "pkgs"):
        shutil.rmtree(root_prefix / "pkgs")

    cmd = [helpers.get_umamba()]
    cmd += ["create", "-n", "env1", "-y"]
    cmd += ["airflow"]
    cmd += ["pytorch"]
    cmd += ["-c", "conda-forge"]

    cmd2 = [helpers.get_umamba(), "create"]
    cmd2 += ["-n", "env2", "-y"]
    cmd2 += ["airflow"]
    cmd2 += ["pytorch"]
    cmd2 += ["-c", "conda-forge"]

    # must not crash
    cmds = [cmd, cmd2]
    procs = [subprocess.Popen(p) for p in cmds]
    for p in procs:
        rc = p.wait()
        assert rc == 0


@pytest.mark.skipif(
    helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
    reason="Running only ultra-dry tests",
)
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize(
    "already_exists, is_conda_env", ((False, False), (True, False), (True, True))
)
@pytest.mark.parametrize("has_specs", (False, True))
def test_create_base(tmp_home, tmp_root_prefix, already_exists, is_conda_env, has_specs):
    if already_exists:
        if is_conda_env:
            (tmp_root_prefix / "conda-meta").mkdir()
    else:
        tmp_root_prefix.rmdir()

    cmd = ["-n", "base"]
    if has_specs:
        cmd += ["xtensor"]

    if already_exists:
        with pytest.raises(subprocess.CalledProcessError):
            helpers.create(*cmd)
    else:
        helpers.create(*cmd)
        assert (tmp_root_prefix / "conda-meta").exists()


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.skipif(
    helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
    reason="Running only ultra-dry tests",
)
@pytest.mark.parametrize("outside_root_prefix", (False, True))
def test_classic_specs(tmp_home, tmp_root_prefix, tmp_path, outside_root_prefix):
    tmp_pkgs_dirs = tmp_path / "cache"
    os.environ["CONDA_PKGS_DIRS"] = str(tmp_pkgs_dirs)
    if outside_root_prefix:
        p = tmp_path / "myenv"
    else:
        p = tmp_root_prefix / "envs" / "myenv"

    res = helpers.create("-p", p, "xtensor", "--json")

    assert res["success"]
    assert res["dry_run"] == (helpers.dry_run_tests == helpers.DryRun.DRY)

    keys = {"success", "prefix", "actions", "dry_run"}
    assert keys.issubset(set(res.keys()))

    action_keys = {"LINK", "PREFIX"}
    assert action_keys.issubset(set(res["actions"].keys()))

    packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
    expected_packages = {"xtensor", "xtl"}
    assert expected_packages.issubset(packages)

    if helpers.dry_run_tests == helpers.DryRun.OFF:
        pkg_name = helpers.get_concrete_pkg(res, "xtensor")
        cached_file = tmp_pkgs_dirs / pkg_name / helpers.xtensor_hpp
        assert cached_file.exists()


@pytest.mark.skipif(
    helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
    reason="Running only ultra-dry tests",
)
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("valid", [False, True])
def test_explicit_specs(tmp_home, tmp_root_prefix, tmp_path, valid):
    spec_file_content = [
        "@EXPLICIT",
        "https://conda.anaconda.org/conda-forge/linux-64/xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
    ]
    if not valid:
        spec_file_content += ["https://conda.anaconda.org/conda-forge/linux-64/xtl"]

    spec_file = tmp_path / "explicit_specs.txt"
    with open(spec_file, "w") as f:
        f.write("\n".join(spec_file_content))

    env_prefix = tmp_path / "myenv"
    cmd = ("-p", env_prefix, "-q", "-f", spec_file)

    if valid:
        helpers.create(*cmd, default_channel=False)

        list_res = helpers.umamba_list("-p", env_prefix, "--json")
        assert len(list_res) == 1
        pkg = list_res[0]
        assert pkg["name"] == "xtensor"
        assert pkg["version"] == "0.21.5"
        assert pkg["build_string"] == "hc9558a2_0"
    else:
        with pytest.raises(subprocess.CalledProcessError):
            helpers.create(*cmd, default_channel=False)


@pytest.mark.skipif(
    helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
    reason="Running only ultra-dry tests",
)
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("prefix_selector", [None, "prefix", "name"])
@pytest.mark.parametrize("create_cmd", ["create", "env create"])
def test_create_empty(tmp_home, tmp_root_prefix, tmp_path, prefix_selector, create_cmd):
    if prefix_selector == "name":
        cmd = ("-n", "myenv", "--json")
        effective_prefix = tmp_root_prefix / "envs" / "myenv"
    elif prefix_selector == "prefix":
        effective_prefix = tmp_path / "some-prefix"
        cmd = ("-p", effective_prefix, "--json")
    else:
        with pytest.raises(subprocess.CalledProcessError):
            helpers.create("--json", create_cmd=create_cmd)
        return

    res = helpers.create(*cmd, create_cmd=create_cmd)

    keys = {"success"}
    assert keys.issubset(set(res.keys()))
    assert res["success"]

    assert (effective_prefix / "conda-meta" / "history").exists()


def test_create_envs_dirs(tmp_root_prefix: Path, tmp_path: Path):
    """Create an environment when the first env dir is not writable."""
    os.environ["CONDA_ENVS_DIRS"] = f"{Path('/noperm')},{tmp_path}"
    env_name = "myenv"
    helpers.create("-n", env_name, "--offline", "--no-rc", no_dry_run=True)
    assert (tmp_path / env_name / "conda-meta" / "history").exists()


@pytest.mark.skipif(
    helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
    reason="Running only ultra-dry tests",
)
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("source", ["cli", "env_var", "rc_file"])
def test_always_yes(tmp_home, tmp_root_prefix, tmp_path, source):
    env_name = "myenv"
    helpers.create("-n", env_name, "xtensor", no_dry_run=True)

    if source == "cli":
        res = helpers.create("-n", env_name, "xtensor", "--json", always_yes=True)
    elif source == "env_var":
        try:
            os.environ["MAMBA_ALWAYS_YES"] = "true"
            res = helpers.create("-n", env_name, "xtensor", "--json", always_yes=False)
        finally:
            os.environ.pop("MAMBA_ALWAYS_YES")
    else:  # rc_file
        rc_file = tmp_path / "config.yaml"
        with open(rc_file, "w") as f:
            f.write("always_yes: true")
        res = helpers.create(
            "-n",
            env_name,
            "xtensor",
            f"--rc-file={rc_file}",
            "--json",
            always_yes=False,
            no_rc=False,
        )

    assert res["success"]
    assert res["dry_run"] == (helpers.dry_run_tests == helpers.DryRun.DRY)


@pytest.mark.skipif(
    helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
    reason="Running only ultra-dry tests",
)
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("relocate_prefix", ["/home/bob/env", "/"])
def test_create_with_relocate_prefix(tmp_home, tmp_root_prefix, tmp_path, relocate_prefix):
    env_prefix = tmp_path / "myenv"
    res = helpers.create(
        "-p",
        env_prefix,
        "--relocate-prefix",
        relocate_prefix,
        "python=3.11",
        "--json",
        no_dry_run=True,
    )
    assert res["success"]
    if platform.system() != "Windows":
        with open(env_prefix / "bin" / "2to3") as f:
            firstline = f.readline()
            assert firstline == f"#!{relocate_prefix}/bin/python3.11\n"


@pytest.mark.skipif(
    helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
    reason="Running only ultra-dry tests",
)
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize(
    "alias",
    [
        None,
        "https://conda.anaconda.org/",
        "https://repo.mamba.pm/",
        "https://repo.mamba.pm",
    ],
)
def test_channel_alias(tmp_home, tmp_root_prefix, alias):
    env_name = "myenv"
    if alias:
        res = helpers.create(
            "-n",
            env_name,
            "xtensor",
            "--json",
            "--channel-alias",
            alias,
        )
        # ca = alias.rstrip("/")
    else:
        res = helpers.create("-n", env_name, "xtensor", "--json")
        # ca = "https://conda.anaconda.org"

    for link in res["actions"]["LINK"]:
        assert link["channel"] == "conda-forge"
        # assert link["channel"].startswith(f"{ca}/conda-forge/")
        # assert link["url"].startswith(f"{ca}/conda-forge/")


@pytest.mark.skipif(
    helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
    reason="Running only ultra-dry tests",
)
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_spec_with_channel(tmp_home, tmp_root_prefix, tmp_path):
    env_name = "myenv"
    res = helpers.create("-n", env_name, "bokeh::bokeh", "--json", "--dry-run")
    ca = "https://conda.anaconda.org"

    for link in res["actions"]["LINK"]:
        if link["name"] == "bokeh":
            assert link["channel"].startswith("bokeh")
            assert link["url"].startswith(f"{ca}/bokeh/")

    spec_file = tmp_path / "env.yaml"
    contents = [
        "dependencies:",
        "  - bokeh::bokeh",
        "  - conda-forge::xtensor 0.22.*",
    ]
    with open(spec_file, "w") as fs:
        fs.write("\n".join(contents))

    res = helpers.create("-n", env_name, "-f", spec_file, "--json", "--dry-run")

    link_packages = [link["name"] for link in res["actions"]["LINK"]]
    assert "bokeh" in link_packages
    assert "xtensor" in link_packages

    for link in res["actions"]["LINK"]:
        if link["name"] == "bokeh":
            assert link["channel"].startswith("bokeh")
            assert link["url"].startswith(f"{ca}/bokeh/")

        if link["name"] == "xtensor":
            assert link["channel"].startswith("conda-forge")
            assert link["url"].startswith(f"{ca}/conda-forge/")
            assert link["version"].startswith("0.22.")


def test_spec_with_channel_and_subdir():
    env_name = "myenv"
    try:
        helpers.create("-n", env_name, "conda-forge/noarch::xtensor", "--dry-run")
    except subprocess.CalledProcessError as e:
        # The error message we are getting today is not the most informative but
        # was needed to unify the solver interface.
        msg = e.stderr.decode()
        assert "The following package could not be installed" in msg
        assert "xtensor" in msg


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_spec_with_multichannel(tmp_home, tmp_root_prefix):
    "https://github.com/mamba-org/mamba/pull/2927"
    helpers.create("-n", "myenv", "defaults::zlib", "--dry-run")


def test_spec_with_slash_in_channel(tmp_home, tmp_root_prefix):
    "https://github.com/mamba-org/mamba/pull/2926"
    with pytest.raises(subprocess.CalledProcessError) as info:
        helpers.create("-n", "env1", "pkgs/main/noarch::python", "--dry-run")

        # The error message we are getting today is not the most informative but
        # was needed to unify the solver interface.
        msg = info.value.stderr.decode()
        assert "The following package could not be installed" in msg
        assert "python" in msg

    os.environ["CONDA_SUBDIR"] = "linux-64"
    helpers.create("-n", "env2", "pkgs/main/linux-64::python", "--dry-run")
    helpers.create("-n", "env3", "pkgs/main::python", "--dry-run")


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_channel_nodefaults(tmp_home, tmp_root_prefix, tmp_path):
    rc_file = tmp_path / "rc.yaml"
    content = [
        "channels:",
        "  - rc",
    ]
    with open(rc_file, "w") as f:
        f.write("\n".join(content))

    spec_file = tmp_path / "env.yaml"
    contents = [
        "channels:",
        "  - yaml",
        "  - nodefaults",
        "dependencies:",
        "  - xtensor-python",
    ]
    with open(spec_file, "w") as f:
        f.write("\n".join(contents))

    res = helpers.create(
        "-n",
        "myenv",
        "-f",
        spec_file,
        "--print-config-only",
        f"--rc-file={rc_file}",
        default_channel=False,
        no_rc=False,
    )

    assert res["channels"] == ["yaml"]


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_pin_applicable(tmp_home, tmp_root_prefix, tmp_path):
    pin_name = "xtensor"
    pin_max_version = "0.24"
    # We add the channel to test a fragile behavior of ``Database``
    spec_name = "conda-forge::xtensor"
    rc_file = tmp_path / "rc.yaml"

    with open(rc_file, "w+") as f:
        f.write(f"""pinned_packages: ["{pin_name}<={pin_max_version}"]""")

    res = helpers.create("-n", "myenv", f"--rc-file={rc_file}", "--json", spec_name, no_rc=False)

    install_pkg = None
    for p in res["actions"]["LINK"]:
        if p["name"] == pin_name:
            install_pkg = p

    # Should do proper version comparison
    assert install_pkg["version"] == "0.24.0"


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_pin_not_applicable(tmp_home, tmp_root_prefix, tmp_path):
    pin_name = "package-that-does-not-exists"
    spec_name = "xtensor"
    rc_file = tmp_path / "rc.yaml"

    with open(rc_file, "w+") as f:
        f.write(f"""pinned_packages: ["{pin_name}"]""")

    res = helpers.create("-n", "myenv", f"--rc-file={rc_file}", "--json", spec_name, no_rc=False)
    assert res["success"] is True
    helpers.get_concrete_pkg(res, spec_name)  # Not trowing


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_set_platform(tmp_home, tmp_root_prefix):
    env_name = "myenv"
    # test a dummy platform/arch
    helpers.create("-n", env_name, "--platform", "ptf-128")
    rc_file = tmp_root_prefix / "envs" / env_name / ".mambarc"
    assert (rc_file).exists()

    rc_dict = None
    with open(rc_file) as f:
        rc_dict = yaml.load(f, Loader=yaml.FullLoader)
    assert rc_dict
    assert set(rc_dict.keys()) == {"platform"}
    assert rc_dict["platform"] == "ptf-128"

    res = helpers.info("-n", env_name, "--json")
    assert "__archspec=1=128" in res["virtual packages"]
    assert res["platform"] == "ptf-128"

    # test virtual packages
    helpers.create("-n", env_name, "--platform", "win-32")
    res = helpers.info("-n", env_name, "--json")
    assert "__archspec=1=x86" in res["virtual packages"]
    assert any(pkg.startswith("__win") for pkg in res["virtual packages"])
    assert res["platform"] == "win-32"


@pytest.mark.skipif(
    helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
    reason="Running only ultra-dry tests",
)
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize(
    "version,build,cache_tag",
    [
        ["3.10", "*_cpython", "cpython-310"],
        # FIXME: https://github.com/mamba-org/mamba/issues/1432
        # [ "3.7", "*_pypy","pypy37"],
    ],
)
def test_pyc_compilation(tmp_home, tmp_root_prefix, version, build, cache_tag):
    env_name = "myenv"
    env_prefix = tmp_root_prefix / "envs" / env_name
    cmd = ["-n", env_name, f"python={version}.*={build}", "six"]

    if platform.system() == "Windows":
        site_packages = env_prefix / "Lib" / "site-packages"
        if version == "2.7":
            cmd += ["-c", "defaults"]  # for vc=9.*
    else:
        site_packages = env_prefix / "lib" / f"python{version}" / "site-packages"

    if cache_tag:
        pyc_fn = Path("__pycache__") / f"six.{cache_tag}.pyc"
    else:
        pyc_fn = Path("six.pyc")

    # Disable pyc compilation to ensure that files are still registered in conda-meta
    helpers.create(*cmd, "--no-pyc")
    assert not (site_packages / pyc_fn).exists()
    six_meta = next((env_prefix / "conda-meta").glob("six-*.json")).read_text()
    assert pyc_fn.name in six_meta

    # Enable pyc compilation to ensure that the pyc files are created
    helpers.create(*cmd)
    assert (site_packages / pyc_fn).exists()
    assert pyc_fn.name in six_meta


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_create_check_dirs(tmp_home, tmp_root_prefix):
    env_name = "myenv"
    env_prefix = tmp_root_prefix / "envs" / env_name
    cmd = ["-n", env_name, "python=3.8", "traitlets"]
    helpers.create(*cmd)

    assert os.path.isdir(env_prefix)

    if platform.system() == "Windows":
        assert os.path.isdir(env_prefix / "lib" / "site-packages" / "traitlets")
    else:
        assert os.path.isdir(env_prefix / "lib" / "python3.8" / "site-packages" / "traitlets")


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("env_file", env_files)
def test_requires_pip_install(tmp_home, tmp_root_prefix, env_file):
    cmd = ["-p", "myenv", "-f", env_file]
    helpers.create(*cmd)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("env_file", env_files)
def test_requires_pip_install_prefix_spaces(tmp_home, tmp_root_prefix, tmp_path, env_file):
    env_prefix = tmp_path / "prefix with space"
    cmd = ["-p", env_prefix, "-f", env_file]
    helpers.create(*cmd)

    if platform.system() != "Windows":
        pip = env_prefix / "bin" / "pip"
        text = pip.read_text()
        lines = text.splitlines()
        assert lines[0] == "#!/bin/sh"
        assert lines[1].startswith("'''exec'")
        version = subprocess.check_output([pip, "--version"])
        assert len(version.decode()) > 0


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("env_file", env_files)
def test_requires_pip_install_no_parent_dir_specified(
    tmp_home, tmp_root_prefix, tmp_path, env_file
):
    initial_working_dir = os.getcwd()
    try:
        # Switch to the current source directory so that the file can be found without
        # using an absolute path
        os.chdir(__this_dir__)
        env_file_name = Path(env_file).name
        cmd = ["-p", tmp_path / "prefix with space", "-f", env_file_name]
        helpers.create(*cmd)
    finally:
        os.chdir(initial_working_dir)  # Switch back to original working dir.


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_pre_commit_compat(tmp_home, tmp_root_prefix, tmp_path):
    # We test compatibility with the downstream pre-commit package here because the pre-commit project does not currently accept any code changes related to Conda, see https://github.com/pre-commit/pre-commit/pull/2446#issuecomment-1353394177.
    def create_repo(path: Path) -> str:
        helpers.subprocess_run("git", "init", cwd=path)
        helpers.subprocess_run("git", "config", "user.email", "test@test", cwd=path)
        helpers.subprocess_run("git", "config", "user.name", "test", cwd=path)
        helpers.subprocess_run("git", "add", ".", cwd=path)
        helpers.subprocess_run("git", "commit", "-m", "Initialize repo", cwd=path)
        return helpers.subprocess_run("git", "rev-parse", "HEAD", cwd=path, text=True).strip()

    hook_repo = tmp_path / "hook_repo"
    caller_repo = tmp_path / "caller_repo"

    # Create hook_repo Git repo
    shutil.copytree(__this_dir__ / "pre_commit_conda_hooks_repo", hook_repo)
    commit_sha = create_repo(hook_repo)

    # Create Git repo to call "pre-commit" from
    pre_commit_config = {
        "repos": [
            {
                "repo": str(hook_repo),
                "rev": commit_sha,
                "hooks": [
                    {"id": "sys-exec"},
                    {
                        "id": "additional-deps",
                        "additional_dependencies": ["psutil", "python=3.11"],
                    },
                ],
            }
        ]
    }
    caller_repo.mkdir()
    pre_commit_config_file = caller_repo / ".pre-commit-config.yaml"
    pre_commit_config_file.write_text(yaml.dump(pre_commit_config))
    (caller_repo / "something.py").write_text("import psutil; print(psutil)")
    create_repo(caller_repo)

    env_prefix = tmp_path / "some-prefix"
    helpers.create("-p", env_prefix, "pre-commit")
    env_overrides = {
        "PRE_COMMIT_USE_MICROMAMBA": "1",
        "PATH": os.pathsep.join(
            [
                str(Path(helpers.get_umamba()).parent),
                *os.environ["PATH"].split(os.pathsep),
            ]
        ),
    }
    try:
        # Need to create some config to set the channel
        with open(tmp_home / ".condarc", "w+") as f:
            f.write("channels: [defaults]")
        output = helpers.umamba_run(
            "-p",
            env_prefix,
            "--cwd",
            caller_repo,
            "pre-commit",
            "run",
            "-v",
            "-a",
            env={**os.environ, **env_overrides},
        )
        assert "conda-default" in output
        assert "<module 'psutil'" in output
    except Exception:
        pre_commit_log = Path.home() / ".cache" / "pre-commit" / "pre-commit.log"
        if pre_commit_log.exists():
            print(pre_commit_log.read_text())
        raise


def test_long_path_support(tmp_home, tmp_root_prefix):
    """Create an environment with a long name."""
    res = helpers.create("-n", "long_prefix_" * 20, "--json")
    assert res["success"]


def get_glibc_version():
    try:
        output = subprocess.check_output(["ldd", "--version"])
    except Exception:
        return
    output.splitlines()
    version = output.splitlines()[0].split()[-1]
    return version.decode("ascii")


@pytest.fixture
def add_glibc_virtual_package():
    version = get_glibc_version()
    with open(__this_dir__ / "channel_a/linux-64/repodata.tpl") as f:
        repodata = f.read()
    with open(__this_dir__ / "channel_a/linux-64/repodata.json", "w") as f:
        if version is not None:
            glibc_placeholder = ', "__glibc=' + version + '"'
        else:
            glibc_placeholder = ""
        repodata = repodata.replace("GLIBC_PLACEHOLDER", glibc_placeholder)
        f.write(repodata)


@pytest.fixture
def copy_channels_osx():
    for channel in ["a", "b"]:
        if not (__this_dir__ / f"channel_{channel}/osx-arm64").exists():
            shutil.copytree(
                __this_dir__ / f"channel_{channel}/linux-64",
                __this_dir__ / f"channel_{channel}/osx-arm64",
            )
            with open(__this_dir__ / f"channel_{channel}/osx-arm64/repodata.json") as f:
                repodata = f.read()
            with open(__this_dir__ / f"channel_{channel}/osx-arm64/repodata.json", "w") as f:
                repodata = repodata.replace("linux-64", "osx-arm64")
                f.write(repodata)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_dummy_create(add_glibc_virtual_package, copy_channels_osx, tmp_home, tmp_root_prefix):
    env_name = "myenv"

    channels = [
        (".", "micromamba", "tests", "channel_b"),
        (".", "micromamba", "tests", "channel_a"),
    ]
    package = "a"
    res = helpers.create_with_chan_pkg(env_name, channels, package)

    for link in res["actions"]["LINK"]:
        assert link["name"] == "a"
        assert link["build"] == "abc"
        assert "channel_b" in link["channel"]

    package = "b"
    res = helpers.create_with_chan_pkg(env_name, channels, package)

    assert any(
        link["name"] == "b" and "channel_a" in link["channel"] for link in res["actions"]["LINK"]
    )

    channels = channels[::-1]
    res = helpers.create_with_chan_pkg(env_name, channels, package)


@pytest.mark.parametrize("use_json", [True, False])
def test_create_dry_run(tmp_home, tmp_root_prefix, use_json):
    env_name = "myenv"
    env_prefix = tmp_root_prefix / "envs" / env_name

    cmd = ["-n", env_name, "--dry-run", "python=3.8"]

    if use_json:
        cmd += ["--json"]

    res = helpers.create(*cmd)

    if not use_json:
        # Assert the non-JSON output is in the terminal.
        assert "Total download" in res

    # dry-run, shouldn't create an environment
    assert not os.path.isdir(env_prefix)


def test_create_with_non_existing_subdir(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "myprefix"
    with pytest.raises(subprocess.CalledProcessError):
        helpers.create("-p", env_prefix, "--dry-run", "--json", "conda-forge/noarch::xtensor")


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_create_with_multiple_files(tmp_home, tmp_root_prefix, tmpdir):
    env_name = "myenv"
    tmp_root_prefix / "envs" / env_name

    # Check that multiple --file arguments are considered
    (tmpdir / "file_a.txt").write(b"a")
    (tmpdir / "file_b.txt").write(b"b")

    res = helpers.create(
        "-n",
        env_name,
        "--json",
        "--override-channels",
        "--strict-channel-priority",
        "--dry-run",
        "-c",
        "./micromamba/tests/channel_b",
        "-c",
        "./micromamba/tests/channel_a",
        "--file",
        str(tmpdir / "file_a.txt"),
        "--file",
        str(tmpdir / "file_b.txt"),
        default_channel=False,
        no_rc=False,
    )

    names = {x["name"] for x in res["actions"]["LINK"]}
    assert names == {"a", "b"}


multichannel_config = {
    "channels": ["conda-forge"],
    "custom_multichannels": {"conda-forge2": ["conda-forge"]},
}


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_create_with_multi_channels(tmp_home, tmp_root_prefix, tmp_path):
    env_name = "myenv"
    tmp_root_prefix / "envs" / env_name

    rc_file = tmp_path / "config.yaml"
    rc_file.write_text(yaml.dump(multichannel_config))

    res = helpers.create(
        "-n",
        env_name,
        "conda-forge2::xtensor",
        "--dry-run",
        "--json",
        f"--rc-file={rc_file}",
        default_channel=False,
        no_rc=False,
    )

    for pkg in res["actions"]["LINK"]:
        assert pkg["url"].startswith("https://conda.anaconda.org/conda-forge/")


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_create_with_multi_channels_and_non_existing_subdir(tmp_home, tmp_root_prefix, tmp_path):
    env_name = "myenv"
    tmp_root_prefix / "envs" / env_name

    rc_file = tmp_path / "config.yaml"
    rc_file.write_text(yaml.dump(multichannel_config))

    with pytest.raises(subprocess.CalledProcessError):
        helpers.create(
            "-n",
            env_name,
            "conda-forge2/noarch::xtensor",
            "--dry-run",
            "--json",
            f"--rc-file={rc_file}",
            default_channel=False,
            no_rc=False,
        )


oci_registry_config = {
    "mirrored_channels": {"oci_channel": ["oci://ghcr.io/channel-mirrors/conda-forge"]},
    # `repodata_use_zst` isn't considered when fetching from oci registries
    # since compressed repodata is handled internally
    # (if present, compressed repodata is necessarily fetched)
    # Setting `repodata_use_zst` to `false` avoids useless requests with
    # zst extension in repodata filename
    "repodata_use_zst": "false",
}


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("spec", ["pandoc", "pandoc=3.1.13"])
@pytest.mark.parametrize("parser", ["mamba", "libsolv"])
def test_create_from_oci_mirrored_channels(tmp_home, tmp_root_prefix, tmp_path, spec, parser):
    env_name = "myenv"
    env_prefix = tmp_root_prefix / "envs" / env_name

    rc_file = tmp_path / "config.yaml"
    rc_file.write_text(yaml.dump(oci_registry_config))

    cmd = ["-n", env_name, spec, "--json", "-c", "oci_channel"]
    if parser == "libsolv":
        cmd += ["--no-exp-repodata-parsing"]

    res = helpers.create(
        *cmd,
        f"--rc-file={rc_file}",
        default_channel=False,
        no_rc=False,
    )
    assert res["success"]

    packages = helpers.umamba_list("-p", env_prefix, "--json")
    assert len(packages) == 1
    pkg = packages[0]
    assert pkg["name"] == "pandoc"
    if spec == "pandoc=3.1.13":
        assert pkg["version"] == "3.1.13"
    assert pkg["base_url"].startswith(
        "https://pkg-containers.githubusercontent.com/ghcr1/blobs/pandoc"
    )


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("parser", ["mamba", "libsolv"])
def test_create_from_oci_mirrored_channels_with_deps(tmp_home, tmp_root_prefix, tmp_path, parser):
    env_name = "myenv"
    env_prefix = tmp_root_prefix / "envs" / env_name

    rc_file = tmp_path / "config.yaml"
    rc_file.write_text(yaml.dump(oci_registry_config))

    cmd = ["-n", env_name, "xtensor", "--json", "-c", "oci_channel"]
    if parser == "libsolv":
        cmd += ["--no-exp-repodata-parsing"]

    res = helpers.create(
        *cmd,
        f"--rc-file={rc_file}",
        default_channel=False,
        no_rc=False,
    )
    assert res["success"]

    packages = helpers.umamba_list("-p", env_prefix, "--json")
    assert len(packages) > 2
    assert any(
        package["name"] == "xtensor"
        and package["base_url"].startswith(
            "https://pkg-containers.githubusercontent.com/ghcr1/blobs/xtensor"
        )
        for package in packages
    )
    assert any(
        package["name"] == "xtl"
        and package["base_url"].startswith(
            "https://pkg-containers.githubusercontent.com/ghcr1/blobs/xtl"
        )
        for package in packages
    )


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("parser", ["mamba", "libsolv"])
def test_create_from_oci_mirrored_channels_pkg_name_mapping(
    tmp_home, tmp_root_prefix, tmp_path, parser
):
    # This is to test Conda/OCI package name mapping
    # Test fetching package from OCI registry with name starting with '_'
    env_name = "myenv"
    env_prefix = tmp_root_prefix / "envs" / env_name

    rc_file = tmp_path / "config.yaml"
    rc_file.write_text(yaml.dump(oci_registry_config))

    cmd = ["-n", env_name, "_go_select", "--json", "-c", "oci_channel"]
    if parser == "libsolv":
        cmd += ["--no-exp-repodata-parsing"]

    res = helpers.create(
        *cmd,
        f"--rc-file={rc_file}",
        default_channel=False,
        no_rc=False,
    )
    assert res["success"]

    packages = helpers.umamba_list("-p", env_prefix, "--json")
    assert len(packages) == 1
    pkg = packages[0]
    assert pkg["name"] == "_go_select"
    assert pkg["base_url"].startswith(
        "https://pkg-containers.githubusercontent.com/ghcr1/blobs/_go_select"
    )


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_create_with_unicode(tmp_home, tmp_root_prefix):
    env_name = "320 áγђß家固êôōçñ한"
    env_prefix = tmp_root_prefix / "envs" / env_name

    res = helpers.create("-n", env_name, "--json", "xtensor", no_rc=False)

    assert res["actions"]["PREFIX"] == str(env_prefix)
    assert any(pkg["name"] == "xtensor" for pkg in res["actions"]["LINK"])
    assert any(pkg["name"] == "xtl" for pkg in res["actions"]["LINK"])


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_create_package_with_non_url_char(tmp_home, tmp_root_prefix):
    """Specific filename char are properly URL encoded.

    Version with epoch such as `x264-1!164.3095-h166bdaf_2.tar.bz2` are not properly URL encoded.

    https://github.com/mamba-org/mamba/issues/3072
    """
    res = helpers.create("-n", "myenv", "-c", "conda-forge", "x264>=1!0", "--json")

    assert any(pkg["name"] == "x264" for pkg in res["actions"]["LINK"])
