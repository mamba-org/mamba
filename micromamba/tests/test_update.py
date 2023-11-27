import os
import platform
import shutil
from pathlib import Path

import pytest

# Need to import everything to get fixtures
from .helpers import *  # noqa: F403
from . import helpers


@pytest.mark.skipif(
    helpers.dry_run_tests == helpers.DryRun.ULTRA_DRY, reason="Running ultra dry tests"
)
class TestUpdate:
    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = helpers.random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + helpers.random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)
    old_version = "0.21.10"
    medium_old_version = "0.22"

    @staticmethod
    @pytest.fixture(scope="class")
    def root(existing_cache):
        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdate.root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdate.prefix

        yield

        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdate.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdate.current_prefix
        shutil.rmtree(TestUpdate.root_prefix)

    @staticmethod
    @pytest.fixture
    def env_created(root):
        if helpers.dry_run_tests == helpers.DryRun.OFF:
            helpers.create(
                f"xtensor={TestUpdate.old_version}",
                "-n",
                TestUpdate.env_name,
                "--json",
                no_dry_run=True,
            )
        res = helpers.umamba_list("xtensor", "-n", TestUpdate.env_name, "--json")
        assert len(res) == 1
        assert res[0]["version"].startswith(TestUpdate.old_version)

        yield TestUpdate.env_name

        shutil.rmtree(TestUpdate.prefix)

    def test_constrained_update(self, env_created):
        update_res = helpers.update(
            "xtensor<=" + self.medium_old_version, "-n", env_created, "--json"
        )
        xtensor_link = [
            to_link for to_link in update_res["actions"]["LINK"] if to_link["name"] == "xtensor"
        ][0]

        assert xtensor_link["version"].startswith(self.medium_old_version)

    # test that we relink noarch packages
    def test_update_python_noarch(self, root):
        if helpers.dry_run_tests == helpers.DryRun.OFF:
            helpers.create(
                "python=3.9",
                "six",
                "requests",
                "-n",
                TestUpdate.env_name,
                "--json",
                no_dry_run=True,
            )
        else:
            return

        res = helpers.umamba_list("python", "-n", TestUpdate.env_name, "--json")
        assert len(res) >= 1
        pyelem = [r for r in res if r["name"] == "python"][0]
        assert pyelem["version"].startswith("3.9")

        res = helpers.umamba_list("requests", "-n", TestUpdate.env_name, "--json")
        prev_requests = [r for r in res if r["name"] == "requests"][0]
        assert prev_requests["version"]

        def site_packages_path(p, pyver):
            if platform.system() == "Windows":
                return os.path.join(self.prefix, "Lib\\site-packages\\", p)
            else:
                return os.path.join(self.prefix, f"lib/python{pyver}/site-packages", p)

        assert os.path.exists(site_packages_path("requests/__pycache__", "3.9"))

        prev_six = helpers.umamba_list("six", "-n", TestUpdate.env_name, "--json")[0]

        update_res = helpers.update("-n", TestUpdate.env_name, "python=3.10", "--json")

        six_link = [
            to_link for to_link in update_res["actions"]["LINK"] if to_link["name"] == "six"
        ][0]

        assert six_link["version"] == prev_six["version"]
        assert six_link["build_string"] == prev_six["build_string"]

        requests_link = [
            to_link for to_link in update_res["actions"]["LINK"] if to_link["name"] == "requests"
        ][0]
        requests_unlink = [
            to_link for to_link in update_res["actions"]["UNLINK"] if to_link["name"] == "requests"
        ][0]

        assert requests_link["version"] == requests_unlink["version"]
        if platform.system() != "Windows":
            assert not os.path.exists(site_packages_path("", "3.9"))
        assert os.path.exists(site_packages_path("requests/__pycache__", "3.10"))

        assert requests_link["version"] == prev_requests["version"]
        assert requests_link["build_string"] == prev_requests["build_string"]

    def test_further_constrained_update(self, env_created):
        update_res = helpers.update("xtensor==0.21.1=*_0", "--json")
        xtensor_link = [
            to_link for to_link in update_res["actions"]["LINK"] if to_link["name"] == "xtensor"
        ][0]

        assert xtensor_link["version"] == "0.21.1"
        assert xtensor_link["build_number"] == 0

    def test_classic_spec(self, env_created):
        update_res = helpers.update("xtensor", "--json", "-n", TestUpdate.env_name)

        xtensor_link = [
            to_link for to_link in update_res["actions"]["LINK"] if to_link["name"] == "xtensor"
        ][0]
        assert TestUpdate.old_version != xtensor_link["version"]

        if helpers.dry_run_tests == helpers.DryRun.OFF:
            pkg = helpers.get_concrete_pkg(update_res, "xtensor")
            pkg_info = helpers.get_concrete_phelpers.kg_info(
                helpers.get_env(TestUpdate.env_name), pkg
            )
            version = pkg_info["version"]

            assert TestUpdate.old_version != version

        # This should do nothing since python is not installed!
        update_res = helpers.update("python", "-n", TestUpdate.env_name, "--json")

        # TODO fix this?!
        assert update_res["message"] == "All requested packages already installed"
        assert update_res["success"] is True
        assert "action" not in update_res

    def test_update_all(self, env_created):
        update_res = helpers.update("--all", "--json")

        xtensor_link = [
            to_link for to_link in update_res["actions"]["LINK"] if to_link["name"] == "xtensor"
        ][0]
        assert TestUpdate.old_version != xtensor_link["version"]

        if helpers.dry_run_tests == helpers.DryRun.OFF:
            pkg = helpers.get_concrete_pkg(update_res, "xtensor")
            pkg_info = helpers.get_concrete_pkg_info(helpers.get_env(TestUpdate.env_name), pkg)
            version = pkg_info["version"]

            assert TestUpdate.old_version != version

            with open(Path(self.prefix) / "conda-meta" / "history") as h:
                history = h.readlines()
            print("".join(history))
            for el in reversed(history):
                x = el.strip()
                if x.startswith(">=="):
                    break
                assert not x.startswith("update specs:")

    @pytest.mark.parametrize(
        "alias",
        [
            None,
            "https://conda.anaconda.org/",
            "https://repo.mamba.pm/",
            "https://repo.mamba.pm",
        ],
    )
    def test_channel_alias(self, alias, env_created):
        if alias:
            res = helpers.update(
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
            res = helpers.update("-n", TestUpdate.env_name, "xtensor", "--json", "--dry-run")
            ca = "https://conda.anaconda.org"

        for to_link in res["actions"]["LINK"]:
            assert to_link["channel"].startswith(f"{ca}/conda-forge/")
            assert to_link["url"].startswith(f"{ca}/conda-forge/")


class TestUpdateConfig:
    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = helpers.random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + helpers.random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    @staticmethod
    @pytest.fixture(scope="class")
    def root(existing_cache):
        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdateConfig.root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdateConfig.prefix
        helpers.create("-n", "base", no_dry_run=True)
        helpers.create("-n", TestUpdateConfig.env_name, "--offline", no_dry_run=True)

        yield

        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdateConfig.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdateConfig.current_prefix
        shutil.rmtree(TestUpdateConfig.root_prefix)

    @staticmethod
    @pytest.fixture
    def env_created(root):
        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdateConfig.root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdateConfig.prefix

        yield

        for v in ("CONDA_CHANNELS", "MAMBA_TARGET_PREFIX"):
            if v in os.environ:
                os.environ.pop(v)

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
    def test_specs(self, source, file_type, env_created):
        cmd = []
        specs = []

        if source in ("cli_only", "both"):
            specs = ["xframe", "xtl"]
            cmd = list(specs)

        if source in ("spec_file_only", "both"):
            f_name = helpers.random_string()
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

        res = helpers.install(*cmd, "--print-config-only")

        TestUpdateConfig.config_tests(res)
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
        env_created,
    ):
        cmd = []

        if root_prefix in (None, "cli"):
            os.environ["MAMBA_DEFAULT_ROOT_PREFIX"] = os.environ.pop("MAMBA_ROOT_PREFIX")

        if root_prefix == "cli":
            cmd += ["-r", TestUpdateConfig.root_prefix]

        r = TestUpdateConfig.root_prefix

        if target_is_root:
            p = r
            n = "base"
        else:
            p = TestUpdateConfig.prefix
            n = TestUpdateConfig.env_name

        expected_p = p

        if cli_prefix:
            cmd += ["-p", p]

        if cli_env_name:
            cmd += ["-n", n]

        if yaml_name:
            f_name = helpers.random_string() + ".yaml"
            spec_file = os.path.join(TestUpdateConfig.prefix, f_name)

            if yaml_name == "prefix":
                yaml_n = p
            else:
                yaml_n = n
                if not (cli_prefix or cli_env_name or target_is_root):
                    expected_p = os.path.join(TestUpdateConfig.root_prefix, "envs", yaml_n)

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
            with pytest.raises(helpers.subprocess.CalledProcessError):
                helpers.install(*cmd, "--print-config-only")
        else:
            res = helpers.install(*cmd, "--print-config-only")
            TestUpdateConfig.config_tests(res, root_prefix=r, target_prefix=expected_p)

    @pytest.mark.parametrize("cli", (False, True))
    @pytest.mark.parametrize("yaml", (False, True))
    @pytest.mark.parametrize("env_var", (False, True))
    @pytest.mark.parametrize("rc_file", (False, True))
    def test_channels(self, cli, yaml, env_var, rc_file, env_created):
        cmd = []
        expected_channels = []

        if cli:
            cmd += ["-c", "cli"]
            expected_channels += ["cli"]

        if yaml:
            f_name = helpers.random_string() + ".yaml"
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
            f_name = helpers.random_string() + ".yaml"
            rc_file = os.path.join(TestUpdateConfig.prefix, f_name)

            file_content = ["channels: [rc]"]
            with open(rc_file, "w") as f:
                f.write("\n".join(file_content))

            cmd += ["--rc-file", rc_file]
            expected_channels += ["rc"]

        res = helpers.install(*cmd, "--print-config-only", no_rc=not rc_file, default_channel=False)
        TestUpdateConfig.config_tests(res)
        if expected_channels:
            assert res["channels"] == expected_channels
        else:
            assert res["channels"] is None

    @pytest.mark.parametrize("type", ("yaml", "classic", "explicit"))
    def test_multiple_spec_files(self, type, env_created):
        cmd = []
        specs = ["xtensor", "xsimd"]
        explicit_specs = [
            "https://conda.anaconda.org/conda-forge/linux-64/xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
            "https://conda.anaconda.org/conda-forge/linux-64/xsimd-7.4.8-hc9558a2_0.tar.bz2#32d5b7ad7d6511f1faacf87e53a63e5f",
        ]

        for i in range(2):
            f_name = helpers.random_string()
            file = os.path.join(TestUpdateConfig.prefix, f_name)

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

        if type == "yaml":
            with pytest.raises(helpers.subprocess.CalledProcessError):
                helpers.install(*cmd, "--print-config-only")
        else:
            res = helpers.install(*cmd, "--print-config-only")
            if type == "classic":
                assert res["specs"] == specs
            else:  # explicit
                assert res["specs"] == [explicit_specs[0]]

    def test_channel_specific(self, env_created):
        helpers.install("quantstack::sphinx", no_dry_run=True)
        res = helpers.update("quantstack::sphinx", "-c", "conda-forge", "--json")
        assert "actions" not in res
