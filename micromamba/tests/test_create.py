import json
import os
import platform
import random
import shutil
import string
import subprocess
from pathlib import Path

import pytest

from .helpers import *

source_dir_path = os.path.dirname(os.path.realpath(__file__))


class TestCreate:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)
    other_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))

    spec_files_location = os.path.expanduser(
        os.path.join("~", "mamba_spec_files_test_" + random_string())
    )

    test_lockfile_path = os.path.realpath(
        os.path.join(source_dir_path, "test_env-lock.yaml")
    )

    @classmethod
    def setup_class(cls):
        assert os.path.exists(TestCreate.test_lockfile_path)
        os.environ["MAMBA_ROOT_PREFIX"] = TestCreate.root_prefix
        os.makedirs(TestCreate.spec_files_location, exist_ok=True)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestCreate.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestCreate.current_prefix

        if Path(TestCreate.spec_files_location).exists():
            shutil.rmtree(TestCreate.spec_files_location)

    @classmethod
    def teardown(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestCreate.root_prefix
        os.environ["CONDA_PREFIX"] = TestCreate.prefix

        for v in ("CONDA_CHANNELS", "MAMBA_TARGET_PREFIX"):
            if v in os.environ:
                os.environ.pop(v)

        if Path(TestCreate.root_prefix).exists():
            shutil.rmtree(TestCreate.root_prefix)
        if Path(TestCreate.other_prefix).exists():
            shutil.rmtree(TestCreate.other_prefix)

    @classmethod
    def config_tests(cls, res, root_prefix, target_prefix):
        assert res["root_prefix"] == root_prefix
        assert res["target_prefix"] == target_prefix
        assert not res["use_target_prefix_fallback"]
        checks = (
            MAMBA_ALLOW_EXISTING_PREFIX
            | MAMBA_NOT_ALLOW_MISSING_PREFIX
            | MAMBA_ALLOW_NOT_ENV_PREFIX
            | MAMBA_NOT_EXPECT_EXISTING_PREFIX
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
        cmd = ["-p", TestCreate.prefix]
        specs = []

        if source in ("cli_only", "both"):
            specs = ["xframe", "xtl"]
            cmd += specs

        if source in ("spec_file_only", "both"):
            f_name = random_string()
            spec_file = os.path.join(TestCreate.spec_files_location, f_name)

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

            os.makedirs(TestCreate.root_prefix, exist_ok=True)
            with open(spec_file, "w") as f:
                f.write("\n".join(file_content))

            cmd += ["-f", spec_file]

        res = create(*cmd, "--print-config-only")

        TestCreate.config_tests(res, TestCreate.root_prefix, TestCreate.prefix)
        assert res["env_name"] == ""
        assert res["specs"] == specs

        json_res = create(*cmd, "--json")
        assert json_res["success"] == True

    def test_lockfile(self):
        cmd_prefix = ["-p", TestCreate.prefix]
        f_name = random_string()
        spec_file = os.path.join(TestCreate.spec_files_location, f_name) + "-lock.yaml"
        shutil.copyfile(TestCreate.test_lockfile_path, spec_file)
        assert os.path.exists(spec_file)

        res = create(*cmd_prefix, "-f", spec_file, "--json")
        assert res["success"] == True

        packages = umamba_list(*cmd_prefix, "--json")
        assert any(
            package["name"] == "zlib" and package["version"] == "1.2.11"
            for package in packages
        )

    @pytest.mark.parametrize("root_prefix", (None, "env_var", "cli"))
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
        self,
        root_prefix,
        target_is_root,
        cli_prefix,
        cli_env_name,
        yaml_name,
        env_var,
        fallback,
        similar_non_canonical,
        non_canonical_position,
        existing_cache,
    ):
        cmd = []

        if root_prefix in (None, "cli"):
            os.environ["MAMBA_DEFAULT_ROOT_PREFIX"] = os.environ.pop(
                "MAMBA_ROOT_PREFIX"
            )

        if root_prefix == "cli":
            cmd += ["-r", TestCreate.root_prefix]

        r = TestCreate.root_prefix

        if target_is_root:
            p = r
            n = "base"
        else:
            p = TestCreate.prefix
            n = TestCreate.env_name

        expected_p = os.path.realpath(p)
        if similar_non_canonical:
            if non_canonical_position == "append":
                p = os.path.join(p, ".")
            else:
                home = os.path.expanduser("~")
                p = p.replace(home, os.path.join(home, "."))

        if cli_prefix:
            cmd += ["-p", p]

        if cli_env_name:
            cmd += ["-n", n]

        if yaml_name:
            f_name = random_string() + ".yaml"
            spec_file = os.path.join(TestCreate.spec_files_location, f_name)

            if yaml_name == "prefix":
                yaml_n = p
            else:
                yaml_n = "yaml_name"
                if not (cli_prefix or cli_env_name):
                    expected_p = os.path.join(TestCreate.root_prefix, "envs", yaml_n)

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
            or not (cli_prefix or cli_env_name or yaml_name or env_var)
        ):
            with pytest.raises(subprocess.CalledProcessError):
                create(*cmd, "--print-config-only")
        else:
            res = create(*cmd, "--print-config-only")
            TestCreate.config_tests(res, root_prefix=r, target_prefix=expected_p)

    @pytest.mark.parametrize("cli", (False, True))
    @pytest.mark.parametrize("yaml", (False, True))
    @pytest.mark.parametrize("env_var", (False, True))
    @pytest.mark.parametrize("rc_file", (False, True))
    def test_channels(self, cli, yaml, env_var, rc_file, existing_cache):
        cmd = ["-p", TestCreate.prefix]
        expected_channels = []

        if cli:
            cmd += ["-c", "cli"]
            expected_channels += ["cli"]

        if yaml:
            f_name = random_string() + ".yaml"
            spec_file = os.path.join(TestCreate.spec_files_location, f_name)

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
            rc_file = os.path.join(TestCreate.spec_files_location, f_name)

            file_content = ["channels: [rc]"]
            with open(rc_file, "w") as f:
                f.write("\n".join(file_content))

            cmd += ["--rc-file", rc_file]
            expected_channels += ["rc"]

        res = create(
            *cmd, "--print-config-only", no_rc=not rc_file, default_channel=False
        )
        TestCreate.config_tests(res, TestCreate.root_prefix, TestCreate.prefix)
        if expected_channels:
            assert res["channels"] == expected_channels
        else:
            assert res["channels"] is None

    @pytest.mark.parametrize("type", ("yaml", "classic", "explicit"))
    def test_multiple_spec_files(self, type, existing_cache):
        cmd = ["-p", TestCreate.prefix]
        specs = ["xtensor", "xsimd"]
        explicit_specs = [
            "https://conda.anaconda.org/conda-forge/linux-64/xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
            "https://conda.anaconda.org/conda-forge/linux-64/xsimd-7.4.8-hc9558a2_0.tar.bz2#32d5b7ad7d6511f1faacf87e53a63e5f",
        ]

        for i in range(2):
            f_name = random_string()
            file = os.path.join(TestCreate.spec_files_location, f_name)

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
                create(*cmd, "--print-config-only")
        else:
            res = create(*cmd, "--print-config-only")
            if type == "classic":
                assert res["specs"] == specs
            else:  # explicit
                assert res["specs"] == [explicit_specs[0]]

    @pytest.mark.skipif(
        dry_run_tests is DryRun.ULTRA_DRY, reason="Running only ultra-dry tests"
    )
    @pytest.mark.parametrize(
        "already_exists, is_conda_env", ((False, False), (True, False), (True, True))
    )
    @pytest.mark.parametrize("has_specs", (False, True))
    def test_create_base(self, already_exists, is_conda_env, has_specs, existing_cache):
        if already_exists:
            if is_conda_env:
                os.makedirs(
                    os.path.join(TestCreate.root_prefix, "conda-meta"), exist_ok=False
                )
            else:
                os.makedirs(TestCreate.root_prefix)

        cmd = ["-n", "base"]
        if has_specs:
            cmd += ["xtensor"]

        if already_exists:
            with pytest.raises(subprocess.CalledProcessError):
                create(*cmd)
        else:
            create(*cmd)
            assert Path(os.path.join(TestCreate.root_prefix, "conda-meta")).exists()

    @pytest.mark.skipif(
        dry_run_tests is DryRun.ULTRA_DRY, reason="Running only ultra-dry tests"
    )
    @pytest.mark.parametrize("outside_root_prefix", (False, True))
    def test_classic_specs(self, outside_root_prefix, existing_cache):
        if outside_root_prefix:
            p = TestCreate.other_prefix
        else:
            p = TestCreate.prefix

        res = create("-p", p, "xtensor", "--json")

        assert res["success"]
        assert res["dry_run"] == (dry_run_tests == DryRun.DRY)

        keys = {"success", "prefix", "actions", "dry_run"}
        assert keys.issubset(set(res.keys()))

        action_keys = {"LINK", "PREFIX"}
        assert action_keys.issubset(set(res["actions"].keys()))

        packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
        expected_packages = {"xtensor", "xtl"}
        assert expected_packages.issubset(packages)

        if dry_run_tests == DryRun.OFF:
            pkg_name = get_concrete_pkg(res, "xtensor")
            cached_file = existing_cache / pkg_name / xtensor_hpp
            assert cached_file.exists()

    @pytest.mark.skipif(
        dry_run_tests is DryRun.ULTRA_DRY, reason="Running only ultra-dry tests"
    )
    @pytest.mark.parametrize("valid", [False, True])
    def test_explicit_specs(self, valid, existing_cache):
        spec_file_content = [
            "@EXPLICIT",
            "https://conda.anaconda.org/conda-forge/linux-64/xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
        ]
        if not valid:
            spec_file_content += ["https://conda.anaconda.org/conda-forge/linux-64/xtl"]

        spec_file = os.path.join(TestCreate.spec_files_location, "explicit_specs.txt")
        with open(spec_file, "w") as f:
            f.write("\n".join(spec_file_content))

        cmd = ("-p", TestCreate.prefix, "-q", "-f", spec_file)

        if valid:
            create(*cmd, default_channel=False)

            list_res = umamba_list("-p", TestCreate.prefix, "--json")
            assert len(list_res) == 1
            pkg = list_res[0]
            assert pkg["name"] == "xtensor"
            assert pkg["version"] == "0.21.5"
            assert pkg["build_string"] == "hc9558a2_0"
        else:
            with pytest.raises(subprocess.CalledProcessError):
                create(*cmd, default_channel=False)

    @pytest.mark.skipif(
        dry_run_tests is DryRun.ULTRA_DRY, reason="Running only ultra-dry tests"
    )
    @pytest.mark.parametrize("prefix_selector", [None, "prefix", "name"])
    def test_create_empty(self, prefix_selector, existing_cache):

        if prefix_selector == "name":
            cmd = ("-n", TestCreate.env_name, "--json")
        elif prefix_selector == "prefix":
            cmd = ("-p", TestCreate.prefix, "--json")
        else:
            with pytest.raises(subprocess.CalledProcessError):
                create("--json")
            return

        res = create(*cmd)

        keys = {"success"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]

        assert Path(os.path.join(TestCreate.prefix, "conda-meta", "history")).exists()

    @pytest.mark.skipif(
        dry_run_tests is DryRun.ULTRA_DRY, reason="Running only ultra-dry tests"
    )
    @pytest.mark.parametrize("source", ["cli", "env_var", "rc_file"])
    def test_always_yes(self, source, existing_cache):
        create("-n", TestCreate.env_name, "xtensor", no_dry_run=True)

        if source == "cli":
            res = create(
                "-n", TestCreate.env_name, "xtensor", "--json", always_yes=True
            )
        elif source == "env_var":
            try:
                os.environ["MAMBA_ALWAYS_YES"] = "true"
                res = create(
                    "-n", TestCreate.env_name, "xtensor", "--json", always_yes=False
                )
            finally:
                os.environ.pop("MAMBA_ALWAYS_YES")
        else:  # rc_file
            rc_file = os.path.join(
                TestCreate.spec_files_location, random_string() + ".yaml"
            )
            with open(rc_file, "w") as f:
                f.write("always_yes: true")
            res = create(
                "-n",
                TestCreate.env_name,
                "xtensor",
                f"--rc-file={rc_file}",
                "--json",
                always_yes=False,
                no_rc=False,
            )

        assert res["success"]
        assert res["dry_run"] == (dry_run_tests == DryRun.DRY)

    @pytest.mark.skipif(
        dry_run_tests is DryRun.ULTRA_DRY, reason="Running only ultra-dry tests"
    )
    @pytest.mark.parametrize(
        "alias",
        [
            None,
            "https://conda.anaconda.org/",
            "https://repo.mamba.pm/",
            "https://repo.mamba.pm",
        ],
    )
    def test_channel_alias(self, alias, existing_cache):
        if alias:
            res = create(
                "-n",
                TestCreate.env_name,
                "xtensor",
                "--json",
                "--channel-alias",
                alias,
            )
            ca = alias.rstrip("/")
        else:
            res = create("-n", TestCreate.env_name, "xtensor", "--json")
            ca = "https://conda.anaconda.org"

        for l in res["actions"]["LINK"]:
            assert l["channel"].startswith(f"{ca}/conda-forge/")
            assert l["url"].startswith(f"{ca}/conda-forge/")

    def test_spec_with_channel(self, existing_cache):
        res = create("-n", TestCreate.env_name, "bokeh::bokeh", "--json", "--dry-run")
        ca = "https://conda.anaconda.org"

        for l in res["actions"]["LINK"]:
            if l["name"] == "bokeh":
                assert l["channel"].startswith(f"{ca}/bokeh/")
                assert l["url"].startswith(f"{ca}/bokeh/")

        f_name = random_string() + ".yaml"
        spec_file = os.path.join(TestCreate.spec_files_location, f_name)

        contents = [
            "dependencies:",
            "  - bokeh::bokeh",
            "  - conda-forge::xtensor 0.22.*",
        ]
        with open(spec_file, "w") as fs:
            fs.write("\n".join(contents))

        res = create("-n", TestCreate.env_name, "-f", spec_file, "--json", "--dry-run")

        link_packages = [l["name"] for l in res["actions"]["LINK"]]
        assert "bokeh" in link_packages
        assert "xtensor" in link_packages

        for l in res["actions"]["LINK"]:
            if l["name"] == "bokeh":
                assert l["channel"].startswith(f"{ca}/bokeh/")
                assert l["url"].startswith(f"{ca}/bokeh/")

            if l["name"] == "xtensor":
                assert l["channel"].startswith(f"{ca}/conda-forge/")
                assert l["url"].startswith(f"{ca}/conda-forge/")
                assert l["version"].startswith("0.22.")

    def test_set_platform(self, existing_cache):
        # test a dummy platform/arch
        create("-n", TestCreate.env_name, "--platform", "ptf-128")
        rc_file = Path(TestCreate.prefix) / ".mambarc"
        assert (rc_file).exists()

        rc_dict = None
        with open(rc_file) as f:
            rc_dict = yaml.load(f, Loader=yaml.FullLoader)
        assert rc_dict
        assert set(rc_dict.keys()) == {"platform"}
        assert rc_dict["platform"] == "ptf-128"

        res = info("-n", TestCreate.env_name, "--json")
        assert "__archspec=1=128" in res["virtual packages"]
        assert res["platform"] == "ptf-128"

        # test virtual packages
        create("-n", TestCreate.env_name, "--platform", "win-32")
        res = info("-n", TestCreate.env_name, "--json")
        assert "__archspec=1=x86" in res["virtual packages"]
        assert "__win=0=0" in res["virtual packages"]
        assert res["platform"] == "win-32"

    @pytest.mark.skipif(
        dry_run_tests is DryRun.ULTRA_DRY, reason="Running only ultra-dry tests"
    )
    @pytest.mark.parametrize(
        "version,build,cache_tag",
        [
            ["2.7", "*", ""],
            ["3.10", "*_cpython", "cpython-310"],
            # FIXME: https://github.com/mamba-org/mamba/issues/1432
            # [ "3.7", "*_pypy","pypy37"],
        ],
    )
    def test_pyc_compilation(self, version, build, cache_tag):
        prefix = Path(TestCreate.prefix)
        cmd = ["-n", TestCreate.env_name, f"python={version}.*={build}", "six"]

        if platform.system() == "Windows":
            site_packages = prefix / "Lib" / "site-packages"
            if version == "2.7":
                cmd += ["-c", "defaults"]  # for vc=9.*
        else:
            site_packages = prefix / "lib" / f"python{version}" / "site-packages"

        if cache_tag:
            pyc_fn = Path("__pycache__") / f"six.{cache_tag}.pyc"
        else:
            pyc_fn = Path(f"six.pyc")

        # Disable pyc compilation to ensure that files are still registered in conda-meta
        create(*cmd, "--no-pyc")
        assert not (site_packages / pyc_fn).exists()
        six_meta = next((prefix / "conda-meta").glob("six-*.json")).read_text()
        assert pyc_fn.name in six_meta

        # Enable pyc compilation to ensure that the pyc files are created
        create(*cmd)
        assert (site_packages / pyc_fn).exists()
        assert pyc_fn.name in six_meta
