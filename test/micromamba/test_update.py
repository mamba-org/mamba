import os
import platform
import shutil
from pathlib import Path

import pytest

from .helpers import *


@pytest.mark.skipif(dry_run_tests == DryRun.ULTRA_DRY, reason="Running ultra dry tests")
class TestUpdate:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)
    old_version = "0.18.3"

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdate.root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdate.prefix

        res = create(
            f"xtensor={TestUpdate.old_version}",
            "-n",
            TestUpdate.env_name,
            "--json",
            no_dry_run=True,
        )

        pkg = get_concrete_pkg(res, "xtensor")
        pkg_info = get_concrete_pkg_info(get_env(TestUpdate.env_name), pkg)
        version = pkg_info["version"]
        assert version.startswith(TestUpdate.old_version)

    @classmethod
    def setup(cls):
        if dry_run_tests == DryRun.OFF:
            install(
                f"xtensor={TestUpdate.old_version}",
                "-n",
                TestUpdate.env_name,
                "--json",
                no_dry_run=True,
            )
        res = umamba_list("xtensor", "-n", TestUpdate.env_name, "--json")
        assert len(res) == 1
        assert res[0]["version"].startswith(TestUpdate.old_version)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdate.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdate.current_prefix
        shutil.rmtree(TestUpdate.root_prefix)

    @pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
    def test_update(self, env_selector):

        if env_selector == "prefix":
            update_res = update("xtensor", "-p", TestUpdate.prefix, "--json")
        elif env_selector == "name":
            update_res = update("xtensor", "-n", TestUpdate.env_name, "--json")
        else:
            update_res = update("xtensor", "--json")

        xtensor_link = [
            l for l in update_res["actions"]["LINK"] if l["name"] == "xtensor"
        ][0]
        assert TestUpdate.old_version != xtensor_link["version"]

        if dry_run_tests == DryRun.OFF:
            pkg = get_concrete_pkg(update_res, "xtensor")
            pkg_info = get_concrete_pkg_info(get_env(TestUpdate.env_name), pkg)
            version = pkg_info["version"]

            assert TestUpdate.old_version != version

        # This should do nothing since python is not installed!
        update_res = update("python", "-n", TestUpdate.env_name, "--json")

        # TODO fix this?!
        assert update_res["message"] == "All requested packages already installed"
        assert update_res["success"] == True
        assert "action" not in update_res

    @pytest.mark.parametrize(
        "alias",
        [
            "",
            "https://conda.anaconda.org/",
            "https://repo.mamba.pm/",
            "https://repo.mamba.pm",
        ],
    )
    def test_channel_alias(self, alias):
        if alias:
            res = update(
                "-n",
                TestUpdate.env_name,
                "xtensor",
                "--json",
                "--dry-run",
                "--channel-alias",
                alias,
            )
            ca = alias.rstrip("/")
        else:
            res = update("-n", TestUpdate.env_name, "xtensor", "--json", "--dry-run")
            ca = "https://conda.anaconda.org"

        for l in res["actions"]["LINK"]:
            assert l["channel"].startswith(f"{ca}/conda-forge/")
            assert l["url"].startswith(f"{ca}/conda-forge/")

    @pytest.mark.parametrize("channels", [None, "both", "CLI_only", "file_only"])
    @pytest.mark.parametrize("env_name", [None, "both", "CLI_only", "file_only"])
    @pytest.mark.parametrize("specs", ["both", "CLI_only", "file_only"])
    def test_yaml_spec_file(self, channels, env_name, specs):
        spec_file_content = []
        if env_name not in ("CLI_only", None):
            spec_file_content += ["name: not_existing_env"]
        if channels not in ("CLI_only", None):
            spec_file_content += ["channels:", "  - https://repo.mamba.pm/conda-forge"]
        if specs != "CLI_only":
            spec_file_content += ["dependencies:", "  - xtensor"]

        yaml_spec_file = os.path.join(TestUpdate.prefix, "yaml_env.yml")
        with open(yaml_spec_file, "w") as f:
            f.write("\n".join(spec_file_content))

        cmd = []

        if specs != "file_only":
            cmd += ["xtl"]

        if env_name not in ("file_only", None):
            cmd += [
                "-n",
                TestUpdate.env_name,
            ]

        if channels not in ("file_only", None):
            cmd += ["-c", "https://conda.anaconda.org/conda-forge"]

        cmd += ["-f", yaml_spec_file, "--json"]

        if specs == "CLI_only" or env_name == "file_only":
            with pytest.raises(subprocess.CalledProcessError):
                update(*cmd, default_channel=False)
        else:
            res = update(*cmd, default_channel=False)

            assert res["success"]
            assert res["dry_run"] == (dry_run_tests == DryRun.DRY)
            assert res["prefix"] == TestUpdate.prefix

            if channels is None:
                # No channels currently only warn on update sub command
                keys = {"success", "prefix", "message", "dry_run"}
                assert keys.issubset(set(res.keys()))
                assert res["message"] == "All requested packages already installed"
            else:
                keys = {"success", "prefix", "actions", "dry_run"}
                assert keys.issubset(set(res.keys()))

                action_keys = {"LINK", "PREFIX"}
                assert action_keys.issubset(set(res["actions"].keys()))

                xtensor_link = [
                    l for l in res["actions"]["LINK"] if l["name"] == "xtensor"
                ][0]
                assert TestUpdate.old_version != xtensor_link["version"]

                if dry_run_tests == DryRun.OFF:
                    pkg = get_concrete_pkg(res, "xtensor")
                    pkg_info = get_concrete_pkg_info(get_env(TestUpdate.env_name), pkg)
                    version = pkg_info["version"]
                    assert TestUpdate.old_version != version

                if channels == "file_only":
                    expected_channel = "https://repo.mamba.pm/conda-forge"
                else:
                    expected_channel = "https://conda.anaconda.org/conda-forge"

                for l in res["actions"]["LINK"]:
                    assert l["channel"].startswith(expected_channel)
                    assert l["url"].startswith(expected_channel)

    @pytest.mark.parametrize("f_count", [1, 2])
    def test_spec_file(self, f_count):
        file_content = [
            "xtensor >=0.20",
            "xsimd",
        ]
        spec_file = os.path.join(TestUpdate.prefix, "file1.txt")
        with open(spec_file, "w") as f:
            f.write("\n".join(file_content))

        file_cmd = ["-f", spec_file]

        if f_count == 2:
            file_content = [
                "python=3.7.*",
                "wheel",
            ]
            spec_file = os.path.join(TestUpdate.prefix, "file2.txt")
            with open(spec_file, "w") as f:
                f.write("\n".join(file_content))
            file_cmd += ["-f", spec_file]

        cmd = ["-p", TestUpdate.prefix, "-q"] + file_cmd

        res = install(*cmd, "--json", default_channel=True)

        assert res["success"]
        assert res["dry_run"] == (dry_run_tests == DryRun.DRY)

        packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
        expected_packages = ["xtensor", "xtl"]
        assert set(expected_packages).issubset(packages)

        packages = {pkg["name"] for pkg in res["actions"]["UNLINK"]}
        assert set(expected_packages).issubset(packages)


class TestUpdateConfig:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdateConfig.root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdateConfig.prefix

        os.makedirs(TestUpdateConfig.root_prefix, exist_ok=False)
        create("-n", TestUpdateConfig.env_name, "--offline", no_dry_run=True)

        # TODO: remove that when https://github.com/mamba-org/mamba/pull/836 will be merge
        os.makedirs(
            os.path.join(TestUpdateConfig.root_prefix, "conda-meta"), exist_ok=False
        )

    @classmethod
    def teardown(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdateConfig.root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdateConfig.prefix

        for v in ("CONDA_CHANNELS", "MAMBA_TARGET_PREFIX"):
            if v in os.environ:
                os.environ.pop(v)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdateConfig.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdateConfig.current_prefix

        shutil.rmtree(TestUpdateConfig.root_prefix)

    @classmethod
    def common_tests(cls, res, root_prefix=root_prefix, target_prefix=prefix):
        assert res["root_prefix"] == root_prefix
        assert res["target_prefix"] == target_prefix
        assert res["use_target_prefix_fallback"]
        checks = (
            MAMBA_ALLOW_ROOT_PREFIX
            | MAMBA_ALLOW_EXISTING_PREFIX
            | MAMBA_NOT_ALLOW_MISSING_PREFIX
            | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX
            | MAMBA_EXPECT_EXISTING_PREFIX
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
    def test_specs(self, source, file_type):
        cmd = []
        specs = []

        if source in ("cli_only", "both"):
            specs = ["xframe", "xtl"]
            cmd = list(specs)

        if source in ("spec_file_only", "both"):
            f_name = random_string()
            spec_file = os.path.join(TestUpdateConfig.root_prefix, f_name)

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
            else:  # yaml
                spec_file += ".yaml"
                file_content = ["dependencies:", "  - xtensor >0.20", "  - xsimd"]
                specs += ["xtensor >0.20", "xsimd"]

            with open(spec_file, "w") as f:
                f.write("\n".join(file_content))

            cmd += ["-f", spec_file]

        res = install(*cmd, "--print-config-only")

        TestUpdateConfig.common_tests(res)
        assert res["env_name"] == ""
        assert res["specs"] == specs

    @pytest.mark.parametrize("root_prefix", (None, "env_var", "cli"))
    @pytest.mark.parametrize("target_is_root", (False, True))
    @pytest.mark.parametrize("cli_prefix", (False, True))
    @pytest.mark.parametrize("cli_env_name", (False, True))
    @pytest.mark.parametrize("yaml", (False, True))
    @pytest.mark.parametrize("env_var", (False, True))
    @pytest.mark.parametrize("fallback", (False, True))
    def test_target_prefix(
        self,
        root_prefix,
        target_is_root,
        cli_prefix,
        cli_env_name,
        yaml,
        env_var,
        fallback,
    ):
        cmd = []

        if root_prefix in (None, "cli"):
            os.environ["MAMBA_DEFAULT_ROOT_PREFIX"] = os.environ.pop(
                "MAMBA_ROOT_PREFIX"
            )

        if root_prefix == "cli":
            cmd += ["-r", TestUpdateConfig.root_prefix]

        r = TestUpdateConfig.root_prefix

        if target_is_root:
            p = r
            n = "base"
        else:
            p = TestUpdateConfig.prefix
            n = TestUpdateConfig.env_name

        if cli_prefix:
            cmd += ["-p", p]

        if cli_env_name:
            cmd += ["-n", n]

        if yaml:
            f_name = random_string() + ".yaml"
            spec_file = os.path.join(TestUpdateConfig.prefix, f_name)

            file_content = [
                f"name: {n}",
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

        if ((cli_prefix or env_var) and (cli_env_name or yaml)) or not (
            cli_prefix or cli_env_name or yaml or env_var or fallback
        ):
            with pytest.raises(subprocess.CalledProcessError):
                install(*cmd, "--print-config-only")
        else:
            res = install(*cmd, "--print-config-only")
            TestUpdateConfig.common_tests(res, root_prefix=r, target_prefix=p)

    @pytest.mark.parametrize("cli", (False, True))
    @pytest.mark.parametrize("yaml", (False, True))
    @pytest.mark.parametrize("env_var", (False, True))
    @pytest.mark.parametrize("rc_file", (False, True))
    def test_channels(self, cli, yaml, env_var, rc_file):
        cmd = []
        expected_channels = []

        if cli:
            cmd += ["-c", "cli"]
            expected_channels += ["cli"]

        if yaml:
            f_name = random_string() + ".yaml"
            spec_file = os.path.join(TestUpdateConfig.prefix, f_name)

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
            f_name = random_string() + ".yaml"
            rc_file = os.path.join(TestUpdateConfig.prefix, f_name)

            file_content = ["channels: [rc]"]
            with open(rc_file, "w") as f:
                f.write("\n".join(file_content))

            cmd += ["--rc-file", rc_file]
            expected_channels += ["rc"]

        res = install(
            *cmd, "--print-config-only", no_rc=not rc_file, default_channel=False
        )
        TestUpdateConfig.common_tests(res)
        if expected_channels:
            assert res["channels"] == expected_channels
        else:
            assert res["channels"] is None

    @pytest.mark.parametrize("type", ("yaml", "classic", "explicit"))
    def test_multiple_spec_files(self, type):
        cmd = []
        specs = ["xtensor", "xsimd"]
        explicit_specs = [
            "https://conda.anaconda.org/conda-forge/linux-64/xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
            "https://conda.anaconda.org/conda-forge/linux-64/xsimd-7.4.8-hc9558a2_0.tar.bz2#32d5b7ad7d6511f1faacf87e53a63e5f",
        ]

        for i in range(2):
            f_name = random_string()
            file = os.path.join(TestUpdateConfig.prefix, f_name)

            if type == "yaml":
                file += ".yaml"
                file_content = [f"dependencies: [{specs[i]}]"]
            elif type == "classic":
                file_content = [specs[i]]
                expected_specs = specs
            else:  # explicit
                file_content = ["@EXPLICIT", explicit_specs[i]]

            with open(file, "w") as f:
                f.write("\n".join(file_content))

            cmd += ["-f", file]

        if type == "yaml":
            with pytest.raises(subprocess.CalledProcessError):
                install(*cmd, "--print-config-only")
        else:
            res = install(*cmd, "--print-config-only")
            if type == "classic":
                assert res["specs"] == specs
            else:  # explicit
                assert res["specs"] == [explicit_specs[0]]
