import os
import shutil
import subprocess
import sys
import platform
from pathlib import Path

import pytest

# Need to import everything to get fixtures
from .helpers import *  # noqa: F403
from . import helpers


class TestInstall:
    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = helpers.random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + helpers.random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.prefix

    @classmethod
    def setup_method(cls):
        helpers.create("-n", TestInstall.env_name, "--offline", no_dry_run=True)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.current_prefix
        shutil.rmtree(TestInstall.root_prefix)

    @classmethod
    def teardown_method(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.prefix
        for v in ("CONDA_CHANNELS", "MAMBA_TARGET_PREFIX"):
            if v in os.environ:
                os.environ.pop(v)

        if Path(TestInstall.prefix).exists():
            helpers.rmtree(TestInstall.prefix)

    @classmethod
    def config_tests(cls, res, root_prefix=root_prefix, target_prefix=prefix):
        assert res["root_prefix"] == root_prefix
        assert res["target_prefix"] == target_prefix
        assert res["use_target_prefix_fallback"]
        checks = (
            helpers.MAMBA_ALLOW_EXISTING_PREFIX
            | helpers.MAMBA_NOT_ALLOW_MISSING_PREFIX
            | helpers.MAMBA_NOT_ALLOW_NOT_ENV_PREFIX
            | helpers.MAMBA_EXPECT_EXISTING_PREFIX
        )
        assert res["target_prefix_checks"] == checks

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
    def test_specs(self, source, file_type, existing_cache):
        cmd = []
        specs = []

        if source in ("cli_only", "both"):
            specs = ["xtensor-python", "xtl"]
            cmd = list(specs)

        if source in ("spec_file_only", "both"):
            f_name = helpers.random_string()
            spec_file = os.path.join(TestInstall.root_prefix, f_name)

            if file_type == "classic":
                file_content = ["xtensor >0.20", "xsimd"]
                specs += file_content
            elif file_type == "explicit":
                channel = "https://conda.anaconda.org/conda-forge/linux-64/"
                explicit_specs = [
                    channel + "xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
                    channel + "xsimd-7.4.8-hc9558a2_0.tar.bz2#32d5b7ad7d6511f1faacf87e53a63e5f",
                ]
                file_content = ["@EXPLICIT"] + explicit_specs
                specs = explicit_specs
            else:  # yaml
                spec_file += ".yaml"
                file_content = ["dependencies:", "  - xtensor >0.20", "  - xsimd"]
                specs += ["xtensor >0.20", "xsimd"]

            with open(spec_file, "w") as f:
                f.write("\n".join(file_content))

            cmd += ["-f", spec_file]

        res = helpers.install(*cmd, "--print-config-only")

        TestInstall.config_tests(res)
        assert res["env_name"] == ""
        assert res["specs"] == specs

    @pytest.mark.parametrize("root_prefix", (None, "env_var", "cli"))
    @pytest.mark.parametrize("target_is_root", (False, True))
    @pytest.mark.parametrize("cli_prefix", (False, True))
    @pytest.mark.parametrize("cli_env_name", (False, True))
    @pytest.mark.parametrize("yaml_name", (False, True, "prefix"))
    @pytest.mark.parametrize("env_var", (False, True))
    @pytest.mark.parametrize("fallback", (False, True))
    def test_target_prefix(
        self,
        root_prefix,
        target_is_root,
        cli_prefix,
        cli_env_name,
        yaml_name,
        env_var,
        fallback,
        existing_cache,
    ):
        cmd = []

        if root_prefix in (None, "cli"):
            os.environ["MAMBA_DEFAULT_ROOT_PREFIX"] = os.environ.pop("MAMBA_ROOT_PREFIX")

        if root_prefix == "cli":
            cmd += ["-r", TestInstall.root_prefix]

        r = TestInstall.root_prefix

        if target_is_root:
            p = r
            n = "base"
        else:
            p = TestInstall.prefix
            n = TestInstall.env_name

        expected_p = p

        if cli_prefix:
            cmd += ["-p", p]

        if cli_env_name:
            cmd += ["-n", n]

        if yaml_name:
            f_name = helpers.random_string() + ".yaml"
            spec_file = os.path.join(TestInstall.prefix, f_name)

            if yaml_name == "prefix":
                yaml_n = p
            else:
                yaml_n = n
                if not (cli_prefix or cli_env_name or target_is_root):
                    expected_p = os.path.join(TestInstall.root_prefix, "envs", yaml_n)

            file_content = [
                f"name: {yaml_n}",
                "dependencies: [xtensor]",
            ]
            with open(spec_file, "w") as f:
                f.write("\n".join(file_content))

            cmd += ["-f", spec_file]

        if env_var:
            os.environ["MAMBA_TARGET_PREFIX"] = p

        if not fallback:
            os.environ.pop("CONDA_PREFIX")
        else:
            os.environ["CONDA_PREFIX"] = p

        if (
            (cli_prefix and cli_env_name)
            or (yaml_name == "prefix")
            or not (cli_prefix or cli_env_name or yaml_name or env_var or fallback)
        ):
            with pytest.raises(subprocess.CalledProcessError):
                helpers.install(*cmd, "--print-config-only")
        else:
            res = helpers.install(*cmd, "--print-config-only")
            TestInstall.config_tests(res, root_prefix=r, target_prefix=expected_p)

    @pytest.mark.parametrize("cli", (False, True))
    @pytest.mark.parametrize("yaml", (False, True))
    @pytest.mark.parametrize("env_var", (False, True))
    @pytest.mark.parametrize("rc_file", (False, True))
    def test_channels(self, cli, yaml, env_var, rc_file, existing_cache):
        cmd = []
        expected_channels = []

        if cli:
            cmd += ["-c", "cli"]
            expected_channels += ["cli"]

        if yaml:
            f_name = helpers.random_string() + ".yaml"
            spec_file = os.path.join(TestInstall.prefix, f_name)

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
            f_name = helpers.random_string() + ".yaml"
            rc_file = os.path.join(TestInstall.prefix, f_name)

            file_content = ["channels: [rc]"]
            with open(rc_file, "w") as f:
                f.write("\n".join(file_content))

            cmd += ["--rc-file", rc_file]
            expected_channels += ["rc"]

        res = helpers.install(*cmd, "--print-config-only", no_rc=not rc_file, default_channel=False)
        TestInstall.config_tests(res)
        if expected_channels:
            assert res["channels"] == expected_channels
        else:
            assert res["channels"] == ["conda-forge"]

    @pytest.mark.parametrize("type", ("yaml", "classic", "explicit"))
    def test_multiple_spec_files(self, type, existing_cache):
        cmd = []
        specs = ["xtensor", "xsimd"]
        channel = "https://conda.anaconda.org/conda-forge/linux-64/"
        explicit_specs = [
            channel + "xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
            channel + "linux-64/xsimd-7.4.8-hc9558a2_0.tar.bz2#32d5b7ad7d6511f1faacf87e53a63e5f",
        ]

        for i in range(2):
            f_name = helpers.random_string()
            file = os.path.join(TestInstall.prefix, f_name)

            if type == "yaml":
                file += ".yaml"
                file_content = [f"dependencies: [{specs[i]}]"]
            elif type == "classic":
                file_content = [specs[i]]
            else:  # explicit
                file_content = ["@EXPLICIT", explicit_specs[i]]

            with open(file, "w") as f:
                f.write("\n".join(file_content))

            cmd += ["-f", file]

        res = helpers.install(*cmd, "--print-config-only")
        if type == "yaml" or type == "classic":
            assert res["specs"] == specs
        else:  # explicit
            assert res["specs"] == [explicit_specs[0]]

    @pytest.mark.parametrize("priority", (None, "disabled", "flexible", "strict"))
    @pytest.mark.parametrize("no_priority", (None, True))
    @pytest.mark.parametrize("strict_priority", (None, True))
    def test_channel_priority(self, priority, no_priority, strict_priority, existing_cache):
        cmd = ["-p", TestInstall.prefix, "xtensor"]
        expected_priority = "flexible"

        if priority:
            cmd += ["--channel-priority", priority]
            expected_priority = priority
        if no_priority:
            cmd += ["--no-channel-priority"]
            expected_priority = "disabled"
        if strict_priority:
            cmd += ["--strict-channel-priority"]
            expected_priority = "strict"

        if (
            (priority is not None)
            and (
                (no_priority and priority != "disabled")
                or (strict_priority and priority != "strict")
            )
            or (no_priority and strict_priority)
        ):
            with pytest.raises(subprocess.CalledProcessError):
                helpers.install(*cmd, "--print-config-only")
        else:
            res = helpers.install(*cmd, "--print-config-only")
            assert res["channel_priority"] == expected_priority

    def test_quotes(self, existing_cache):
        cmd = ["-p", f"{TestInstall.prefix}", "xtensor", "--print-config-only"]
        res = helpers.install(*cmd)
        assert res["target_prefix"] == TestInstall.prefix

    @pytest.mark.parametrize("prefix", ("target", "root"))
    def test_expand_user(self, prefix, existing_cache):
        if prefix == "target":
            r = TestInstall.root_prefix
            p = TestInstall.prefix.replace(os.path.expanduser("~"), "~")
        else:
            r = TestInstall.root_prefix.replace(os.path.expanduser("~"), "~")
            p = TestInstall.prefix

        cmd = [
            "-r",
            r,
            "-p",
            p,
            "xtensor",
            "--print-config-only",
        ]
        res = helpers.install(*cmd)
        assert res["target_prefix"] == TestInstall.prefix
        assert res["root_prefix"] == TestInstall.root_prefix

    def test_empty_specs(self, existing_cache):
        assert "Nothing to do." in helpers.install().strip()

    @pytest.mark.skipif(
        helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
        reason="Running only ultra-dry tests",
    )
    @pytest.mark.parametrize("already_installed", [False, True])
    def test_non_explicit_spec(self, already_installed, existing_cache):
        cmd = ["-p", TestInstall.prefix, "xtensor", "--json"]

        if already_installed:
            helpers.install(*cmd, no_dry_run=True)

        res = helpers.install(*cmd)

        assert res["success"]
        assert res["dry_run"] == (helpers.dry_run_tests == helpers.DryRun.DRY)
        if already_installed:
            keys = {"dry_run", "success", "prefix", "message"}
            assert keys.issubset(set(res.keys()))
        else:
            keys = {"success", "prefix", "actions", "dry_run"}
            assert keys.issubset(set(res.keys()))

            action_keys = {"LINK", "PREFIX"}
            assert action_keys.issubset(set(res["actions"].keys()))

            packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
            expected_packages = {"xtensor", "xtl"}
            assert expected_packages.issubset(packages)

            if not helpers.dry_run_tests:
                pkg_name = helpers.get_concrete_pkg(res, "xtensor")
                orig_file_path = helpers.get_pkg(
                    pkg_name, helpers.xtensor_hpp, TestInstall.current_root_prefix
                )
                assert orig_file_path.exists()

    @pytest.mark.skipif(
        helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
        reason="Running only ultra-dry tests",
    )
    @pytest.mark.parametrize("already_installed", [False, True])
    @pytest.mark.parametrize("valid", [False, True])
    def test_explicit_specs(self, already_installed, valid, existing_cache):
        spec_file_content = [
            "@EXPLICIT",
            "https://conda.anaconda.org/conda-forge/linux-64/xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
        ]
        if not valid:
            spec_file_content += ["https://conda.anaconda.org/conda-forge/linux-64/xtl"]

        spec_file = os.path.join(TestInstall.root_prefix, "explicit_specs.txt")
        with open(spec_file, "w") as f:
            f.write("\n".join(spec_file_content))

        cmd = ("-p", TestInstall.prefix, "-q", "-f", spec_file)

        if valid:
            helpers.install(*cmd, default_channel=False)

            list_res = helpers.umamba_list("-p", TestInstall.prefix, "--json")
            assert len(list_res) == 1
            pkg = list_res[0]
            assert pkg["name"] == "xtensor"
            assert pkg["version"] == "0.21.5"
            assert pkg["build_string"] == "hc9558a2_0"
        else:
            with pytest.raises(subprocess.CalledProcessError):
                helpers.install(*cmd, default_channel=False)

    @pytest.mark.skipif(
        helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
        reason="Running only ultra-dry tests",
    )
    @pytest.mark.parametrize(
        "alias",
        [
            "",
            "https://conda.anaconda.org/",
            "https://repo.mamba.pm/",
            "https://repo.mamba.pm",
        ],
    )
    def test_channel_alias(self, alias, existing_cache):
        if alias:
            res = helpers.install("xtensor", "--json", "--channel-alias", alias)
        else:
            res = helpers.install("xtensor", "--json")

        for to_link in res["actions"]["LINK"]:
            assert to_link["channel"] == "conda-forge"

    @pytest.mark.skipif(
        helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
        reason="Running only ultra-dry tests",
    )
    def test_no_python_pinning(self, existing_cache):
        helpers.install("python=3.9.19", no_dry_run=True)
        res = helpers.install("setuptools=63.4.3", "--no-py-pin", "--json")

        keys = {"success", "prefix", "actions", "dry_run"}
        assert keys.issubset(set(res.keys()))

        action_keys = {"LINK", "UNLINK", "PREFIX"}
        assert action_keys.issubset(set(res["actions"].keys()))

        expected_link_packages = {"python"}
        link_packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
        assert expected_link_packages.issubset(link_packages)
        unlink_packages = {pkg["name"] for pkg in res["actions"]["UNLINK"]}
        assert {"python"}.issubset(unlink_packages)

        py_pkg = [pkg for pkg in res["actions"]["LINK"] if pkg["name"] == "python"][0]
        assert py_pkg["version"] != ("3.9.19")

        py_pkg = [pkg for pkg in res["actions"]["UNLINK"] if pkg["name"] == "python"][0]
        assert py_pkg["version"] == ("3.9.19")

    @pytest.mark.skipif(
        helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
        reason="Running only ultra-dry tests",
    )
    @pytest.mark.skipif(
        sys.platform == "win32" or (sys.platform == "darwin" and platform.machine() == "arm64"),
        reason="Python2 no available",
    )
    def test_python_pinning(self, existing_cache):
        """Black fails to install as it is not available for pinned Python 2."""
        res = helpers.install("python=2", "--json", no_dry_run=True)
        assert res["success"]
        # We do not have great way to check for the type of error for now
        try:
            helpers.install("black", "--py-pin", "--json")
            assert False
        except subprocess.CalledProcessError:
            pass

    @pytest.mark.skipif(
        helpers.dry_run_tests is helpers.DryRun.ULTRA_DRY,
        reason="Running only ultra-dry tests",
    )
    def test_freeze_installed(self, existing_cache):
        helpers.install("xtensor=0.24", no_dry_run=True)
        res = helpers.install("xtensor-blas", "--freeze-installed", "--json")

        # without freeze installed, xtensor-blas 0.21.0 should be installed and xtensor updated to 0.25
        keys = {"success", "prefix", "actions", "dry_run"}
        assert keys.issubset(set(res.keys()))

        action_keys = {"LINK", "PREFIX"}
        assert action_keys.issubset(set(res["actions"].keys()))

        expected_packages = {"xtensor-blas"}
        link_packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
        assert expected_packages.issubset(link_packages)
        assert res["actions"]["LINK"][-1]["version"] == "0.20.0"

    def test_channel_specific(self, existing_cache):
        res = helpers.install("conda-forge::xtensor", "--json", default_channel=False, no_rc=True)

        keys = {"success", "prefix", "actions", "dry_run"}
        assert keys.issubset(set(res.keys()))

        action_keys = {"LINK", "PREFIX"}
        assert action_keys.issubset(set(res["actions"].keys()))

        expected_packages = {"xtensor", "xtl"}
        link_packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
        assert expected_packages.issubset(link_packages)

        for pkg in res["actions"]["LINK"]:
            assert pkg["channel"] == "conda-forge"

    def test_explicit_noarch(self, existing_cache):
        helpers.install("python", no_dry_run=True)

        channel = "https://conda.anaconda.org/conda-forge/noarch/"
        explicit_spec = (
            channel + "appdirs-1.4.4-pyh9f0ad1d_0.tar.bz2#5f095bc6454094e96f146491fd03633b"
        )
        file_content = ["@EXPLICIT", explicit_spec]

        spec_file = os.path.join(TestInstall.root_prefix, "explicit_specs_no_arch.txt")
        with open(spec_file, "w") as f:
            f.write("\n".join(file_content))

        cmd = ("-p", TestInstall.prefix, "-q", "-f", spec_file)

        helpers.install(*cmd, default_channel=False)

        list_res = helpers.umamba_list("-p", TestInstall.prefix, "--json")
        pkgs = [p for p in list_res if p["name"] == "appdirs"]
        assert len(pkgs) == 1
        pkg = pkgs[0]
        assert pkg["version"] == "1.4.4"
        assert pkg["build_string"] == "pyh9f0ad1d_0"

    def test_broken_package_name(self):
        non_existing_url = "https://026e9ab9-6b46-4285-ae0d-427553801720.de/mypackage.tar.bz2"
        try:
            helpers.install(non_existing_url, default_channel=False)
        except subprocess.CalledProcessError as e:
            assert 'Missing name and version in filename "mypackage.tar.bz2"' in e.stderr.decode(
                "utf-8"
            )

    def test_no_reinstall(self, existing_cache):
        """Reinstalling is a no op."""
        res = helpers.install("xtensor", "--json")
        assert "xtensor" in {pkg["name"] for pkg in res["actions"]["LINK"]}

        reinstall_res = helpers.install("xtensor", "--json")
        assert "actions" not in reinstall_res

    def install_local_package(self):
        """Attempts to install a .tar.bz2 package from a local directory."""
        file_path = Path(__file__).parent / "data" / "cph_test_data-0.0.1-0.tar.bz2"

        res = helpers.install(f"file://{file_path}", "--json", default_channel=False)
        assert "cph_test_data" in {pkg["name"] for pkg in res["actions"]["LINK"]}

    def test_force_reinstall(self, existing_cache):
        """Force reinstall installs existing package again."""
        res = helpers.install("xtensor", "--json")
        assert "xtensor" in {pkg["name"] for pkg in res["actions"]["LINK"]}

        reinstall_res = helpers.install("xtensor", "--force-reinstall", "--json")
        assert "xtensor" in {pkg["name"] for pkg in reinstall_res["actions"]["LINK"]}

    def test_force_reinstall_not_installed(self, existing_cache):
        """Force reinstall on non-installed packages is valid."""
        reinstall_res = helpers.install("xtensor", "--force-reinstall", "--json")
        assert "xtensor" in {pkg["name"] for pkg in reinstall_res["actions"]["LINK"]}


def test_install_check_dirs(tmp_home, tmp_root_prefix):
    env_name = "myenv"
    env_prefix = tmp_root_prefix / "envs" / env_name

    helpers.create("-n", env_name, "python=3.8")
    res = helpers.install("-n", env_name, "nodejs", "--json")

    assert os.path.isdir(env_prefix)
    assert "nodejs" in {pkg["name"] for pkg in res["actions"]["LINK"]}

    if helpers.platform.system() == "Windows":
        assert os.path.isdir(env_prefix / "lib" / "site-packages")
    else:
        assert os.path.isdir(env_prefix / "lib" / "python3.8" / "site-packages")


@pytest.mark.skipif(
    sys.platform == "darwin" and platform.machine() == "arm64",
    reason="Python 3.7.9 not available",
)
def test_track_features(tmp_home, tmp_root_prefix):
    env_name = "myenv"
    tmp_root_prefix / "envs" / env_name

    # should install CPython since PyPy has track features
    version = "3.7.9"
    helpers.create("-n", env_name, default_channel=False, no_rc=False)
    helpers.install(
        "-n",
        env_name,
        "-q",
        f"python={version}",
        "--strict-channel-priority",
        no_rc=False,
    )
    res = helpers.umamba_run("-n", env_name, "python", "-c", "import sys; print(sys.version)")
    if helpers.platform.system() == "Windows":
        assert res.strip().startswith(version)
        assert "[MSC v." in res.strip()
    elif helpers.platform.system() == "Linux":
        assert res.strip().startswith(version)
        assert "[GCC" in res.strip()
    else:
        assert res.strip().startswith(version)
        assert "[Clang" in res.strip()

    if helpers.platform.system() == "Linux":
        # now force PyPy install
        helpers.install(
            "-n",
            env_name,
            "-q",
            f"python={version}=*pypy",
            "--strict-channel-priority",
            no_rc=False,
        )
        res = helpers.umamba_run("-n", env_name, "python", "-c", "import sys; print(sys.version)")

        assert res.strip().startswith(version)
        assert "[PyPy" in res.strip()


def test_reinstall_with_new_version(tmp_home, tmp_root_prefix):
    env_name = "myenv"
    tmp_root_prefix / "envs" / env_name

    version = "3.8"
    helpers.create("-n", env_name, default_channel=False, no_rc=False)
    helpers.install(
        "-n",
        env_name,
        "-q",
        f"python={version}",
        "pip",
        no_rc=False,
    )

    res = helpers.umamba_run("-n", env_name, "python", "-c", "import sys; print(sys.version)")
    assert version in res

    res = helpers.umamba_run("-n", env_name, "python", "-c", "import pip; print(pip.__version__)")
    assert len(res)

    # Update python version
    version = "3.9"
    helpers.install(
        "-n",
        env_name,
        "-q",
        f"python={version}",
        no_rc=False,
    )

    res = helpers.umamba_run("-n", env_name, "python", "-c", "import sys; print(sys.version)")
    assert version in res

    res = helpers.umamba_run("-n", env_name, "python", "-c", "import pip; print(pip.__version__)")
    assert len(res)
