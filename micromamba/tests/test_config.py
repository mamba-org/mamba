import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

import pytest
import yaml

from .helpers import create, get_umamba, random_string, remove


def config(*args):
    umamba = get_umamba()
    cmd = [umamba, "config"] + [arg for arg in args if arg]
    res = subprocess.check_output(cmd)
    if "--json" in args:
        j = yaml.load(res, yaml.FullLoader)
        return j

    return res.decode()


@pytest.fixture
def rc_file_path(tmpdir_factory):
    return Path(tmpdir_factory.mktemp("umamba").join("config.yaml"))


@pytest.fixture
def rc_file_args(rc_file_path):
    rc_file_path.write_text("channels:\n  - channel1\n  - channel2\n")
    return ["--rc-file", rc_file_path]


class TestConfig:
    def test_empty(self):
        assert "Configuration of micromamba" in config()

    @pytest.mark.parametrize("quiet_flag", ["-q", "--quiet"])
    def test_quiet(self, quiet_flag):
        assert config(quiet_flag) == ""


class TestConfigSources:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    home_dir = os.path.expanduser("~")
    rc_files = [
        # "/etc/conda/.condarc",
        # "/etc/conda/condarc",
        # "/etc/conda/condarc.d/",
        # "/etc/conda/.mambarc",
        # "/var/lib/conda/.condarc",
        # "/var/lib/conda/condarc",
        # "/var/lib/conda/condarc.d/",
        # "/var/lib/conda/.mambarc",
        os.path.join(root_prefix, ".condarc"),
        os.path.join(root_prefix, "condarc"),
        os.path.join(root_prefix, "condarc.d"),
        os.path.join(root_prefix, ".mambarc"),
        os.path.join(home_dir, ".conda", ".condarc"),
        os.path.join(home_dir, ".conda", "condarc"),
        os.path.join(home_dir, ".conda", "condarc.d"),
        os.path.join(home_dir, ".condarc"),
        os.path.join(home_dir, ".mambarc"),
        os.path.join(prefix, ".condarc"),
        os.path.join(prefix, "condarc"),
        os.path.join(prefix, "condarc.d"),
        os.path.join(prefix, ".mambarc"),
    ]

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestConfigSources.root_prefix
        os.environ["CONDA_PREFIX"] = TestConfigSources.prefix

        os.makedirs(TestConfigSources.root_prefix, exist_ok=False)
        create(
            "-n",
            TestConfigSources.env_name,
            "--json",
            "--offline",
            no_dry_run=True,
        )

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestConfigSources.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestConfigSources.current_prefix
        shutil.rmtree(TestConfigSources.root_prefix)

    @pytest.mark.parametrize("quiet_flag", ["-q", "--quiet"])
    @pytest.mark.parametrize("rc_file", ["", "dummy.yaml", ".mambarc"])
    @pytest.mark.parametrize("norc", [False, True])
    def test_sources(self, quiet_flag, rc_file, norc):
        if rc_file:
            rc_path = os.path.join(TestConfigSources.prefix, rc_file)
            with open(rc_path, "w") as f:
                f.write("override_channels_enabled: true")

            if norc:
                with pytest.raises(subprocess.CalledProcessError):
                    config("sources", quiet_flag, "--rc-file", rc_path, "--no-rc")
            else:
                res = config("sources", quiet_flag, "--rc-file", rc_path)
                rc_path_short = rc_path.replace(os.path.expanduser("~"), "~")
                assert (
                    res.strip().splitlines()
                    == f"Configuration files (by precedence order):\n{rc_path_short}".splitlines()
                )
        else:
            if norc:
                res = config("sources", quiet_flag, "--no-rc")
                assert res.strip() == "Configuration files disabled by --no-rc flag"
            else:
                res = config("sources", quiet_flag)
                assert res.startswith("Configuration files (by precedence order):")

    # TODO: test OS specific sources
    # TODO: test system located sources?
    @pytest.mark.parametrize("rc_file", rc_files)
    def test_rc_file(self, rc_file):
        tmpfiles = []
        for file in TestConfigSources.rc_files:
            folder, f = file.rsplit(os.path.sep, 1)

            if not Path(folder).exists():
                os.makedirs(folder, exist_ok=True)

            if file.endswith(".d") and Path(file).exists() and Path(file).is_dir():
                file = os.path.join(file, "test.yaml")

            if Path(file).exists() and Path(file).is_file():
                tmp_file = os.path.join(folder, "tmp_" + f)
                if Path(tmp_file).exists():
                    if Path(tmp_file).is_dir():
                        shutil.rmtree(tmp_file)
                    else:
                        os.remove(tmp_file)

                os.rename(file, tmp_file)
                tmpfiles.append((file, tmp_file))

        if rc_file.endswith(".d"):
            os.makedirs(rc_file, exist_ok=True)
            rc_file = os.path.join(rc_file, "test.yaml")

        with open(os.path.expanduser(rc_file), "w") as f:
            f.write("override_channels_enabled: true")

        srcs = (
            config(
                "sources",
                "-n",
                TestConfigSources.env_name,
            )
            .strip()
            .splitlines()
        )
        short_name = rc_file.replace(os.path.expanduser("~"), "~")
        expected_srcs = (
            f"Configuration files (by precedence order):\n{short_name}".splitlines()
        )
        assert srcs == expected_srcs

        if rc_file.endswith(".d"):
            shutil.rmtree(rc_file)
        else:
            os.remove(rc_file)

        for (file, tmp_file) in tmpfiles:
            os.rename(tmp_file, file)

    def test_expand_user(self):
        rc_path = os.path.join(TestConfigSources.root_prefix, random_string() + ".yml")
        rc_path_short = rc_path.replace(os.path.expanduser("~"), "~")

        os.makedirs(TestConfigSources.root_prefix, exist_ok=True)
        with open(rc_path, "w") as f:
            f.write("override_channels_enabled: true")

        res = config("sources", "--rc-file", f"{rc_path_short}")
        assert (
            res.strip().splitlines()
            == f"Configuration files (by precedence order):\n{rc_path_short}".splitlines()
        )


class TestConfigList:
    def test_list_with_rc(self, rc_file_args):
        assert (
            config("list", "--no-env", *rc_file_args).splitlines()[:-1]
            == "channels:\n  - channel1\n  - channel2\n".splitlines()
        )

    def test_list_without_rc(self):
        assert config("list", "--no-env", "--no-rc").splitlines()[:-1] == []

    @pytest.mark.parametrize("source_flag", ["--sources", "-s"])
    def test_list_with_sources(self, rc_file_args, source_flag):
        home_folder = os.path.expanduser("~")
        src = f"  # '{str(rc_file_args[-1]).replace(home_folder, '~')}'"
        assert (
            config("list", "--no-env", *rc_file_args, source_flag).splitlines()[:-1]
            == f"channels:\n  - channel1{src}\n  - channel2{src}\n".splitlines()
        )

    @pytest.mark.parametrize("desc_flag", ["--descriptions", "-d"])
    def test_list_with_descriptions(self, rc_file_args, desc_flag):
        assert (
            config("list", "--no-env", *rc_file_args, desc_flag).splitlines()[:-4]
            == f"# channels\n#   Define the list of channels\nchannels:\n"
            "  - channel1\n  - channel2\n".splitlines()
        )

    @pytest.mark.parametrize("desc_flag", ["--long-descriptions", "-l"])
    def test_list_with_long_descriptions(self, rc_file_args, desc_flag):
        assert (
            config("list", "--no-env", *rc_file_args, desc_flag).splitlines()[:-4]
            == f"# channels\n#   The list of channels where the packages will be searched for.\n"
            "#   See also 'channel_priority'.\nchannels:\n  - channel1\n  - channel2\n".splitlines()
        )

    @pytest.mark.parametrize("group_flag", ["--groups", "-g"])
    def test_list_with_groups(self, rc_file_args, group_flag):
        group = (
            "# ######################################################\n"
            "# #               Channels Configuration               #\n"
            "# ######################################################\n\n"
        )

        assert (
            config("list", "--no-env", *rc_file_args, "-d", group_flag).splitlines()[
                :-8
            ]
            == f"{group}# channels\n#   Define the list of channels\nchannels:\n"
            "  - channel1\n  - channel2\n".splitlines()
        )

    def test_env_vars(self):
        os.environ["MAMBA_OFFLINE"] = "true"
        assert (
            config("list", "offline", "--no-rc", "-s").splitlines()
            == "offline: true  # 'MAMBA_OFFLINE'".splitlines()
        )

        os.environ["MAMBA_OFFLINE"] = "false"
        assert (
            config("list", "offline", "--no-rc", "-s").splitlines()
            == "offline: false  # 'MAMBA_OFFLINE'".splitlines()
        )
        os.environ.pop("MAMBA_OFFLINE")

    def test_no_env(self):

        os.environ["MAMBA_OFFLINE"] = "false"

        assert (
            config(
                "list", "offline", "--no-rc", "--no-env", "-s", "--offline"
            ).splitlines()
            == "offline: true  # 'CLI'".splitlines()
        )

        os.environ.pop("MAMBA_OFFLINE")

    def test_precedence(self):
        rc_dir = os.path.expanduser(os.path.join("~", "test_mamba", random_string()))
        os.makedirs(rc_dir, exist_ok=True)
        rc_file = os.path.join(rc_dir, ".mambarc")
        short_rc_file = rc_file.replace(os.path.expanduser("~"), "~")

        with open(rc_file, "w") as f:
            f.write("offline: true")

        try:
            if "MAMBA_OFFLINE" in os.environ:
                os.environ.pop("MAMBA_OFFLINE")

            assert (
                config("list", "offline", f"--rc-file={rc_file}", "-s").splitlines()
                == f"offline: true  # '{short_rc_file}'".splitlines()
            )

            os.environ["MAMBA_OFFLINE"] = "false"
            assert (
                config("list", "offline", "--no-rc", "-s").splitlines()
                == f"offline: false  # 'MAMBA_OFFLINE'".splitlines()
            )
            assert (
                config("list", "offline", f"--rc-file={rc_file}", "-s").splitlines()
                == f"offline: false  # 'MAMBA_OFFLINE' > '{short_rc_file}'".splitlines()
            )

            assert (
                config(
                    "list", "offline", f"--rc-file={rc_file}", "-s", "--offline"
                ).splitlines()
                == f"offline: true  # 'CLI' > 'MAMBA_OFFLINE' > '{short_rc_file}'".splitlines()
            )
            assert (
                config(
                    "list",
                    "offline",
                    f"--rc-file={rc_file}",
                    "--no-env",
                    "-s",
                    "--offline",
                ).splitlines()
                == f"offline: true  # 'CLI' > '{short_rc_file}'".splitlines()
            )
            assert (
                config(
                    "list",
                    "offline",
                    "--no-rc",
                    "--no-env",
                    "-s",
                    "--offline",
                ).splitlines()
                == "offline: true  # 'CLI'".splitlines()
            )
        finally:
            if "MAMBA_OFFLINE" in os.environ:
                os.environ.pop("MAMBA_OFFLINE")
            shutil.rmtree(os.path.expanduser(os.path.join("~", "test_mamba")))


# TODO: instead of "Key is not present in file" => "Key " + key + "is not present in file"
class TestConfigModifiers:
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))

    # vars --file
    home_rc_path = os.path.join(root_prefix, ".dummyrc")

    # vars for --env
    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    prefix = os.path.join(root_prefix, "envs", env_name)
    other_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))

    spec_files_location = os.path.expanduser(
        os.path.join("~", "mamba_spec_files_test_" + random_string())
    )

    @classmethod
    def setup(cls):
        # setup for --env
        os.environ["MAMBA_ROOT_PREFIX"] = TestConfigModifiers.root_prefix
        os.environ["CONDA_PREFIX"] = TestConfigModifiers.prefix

        # speed-up the tests
        os.environ["CONDA_PKGS_DIRS"] = TestConfigModifiers.cache

        os.makedirs(TestConfigModifiers.spec_files_location, exist_ok=True)

        # setup for --file
        os.mkdir(TestConfigModifiers.root_prefix)
        with open(TestConfigModifiers.home_rc_path, "a"):
            os.utime(TestConfigModifiers.home_rc_path)

    @classmethod
    def teardown(cls):
        # teardown for --env
        os.environ["MAMBA_ROOT_PREFIX"] = TestConfigModifiers.root_prefix
        os.environ["CONDA_PREFIX"] = TestConfigModifiers.prefix

        for v in ("CONDA_CHANNELS", "MAMBA_TARGET_PREFIX"):
            if v in os.environ:
                os.environ.pop(v)

        if Path(TestConfigModifiers.root_prefix).exists():
            shutil.rmtree(TestConfigModifiers.root_prefix)
        if Path(TestConfigModifiers.other_prefix).exists():
            shutil.rmtree(TestConfigModifiers.other_prefix)

        # teardown for --file
        if os.path.exists(TestConfigModifiers.home_rc_path):
            os.remove(TestConfigModifiers.home_rc_path)

    def test_file_set_single_input(self):
        config("set", "json", "true", "--file", TestConfigModifiers.home_rc_path)
        assert (
            config(
                "get", "json", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "json: true".splitlines()
        )

    def test_file_set_change_key_value(self):
        config("set", "json", "true", "--file", TestConfigModifiers.home_rc_path)
        config("set", "json", "false", "--file", TestConfigModifiers.home_rc_path)
        assert (
            config(
                "get", "json", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "json: false".splitlines()
        )

    def test_file_set_invalit_input(self):
        assert (
            config(
                "set", "$%#@abc", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "Key is invalid or more than one key was received".splitlines()
        )

    def test_file_set_multiple_inputs(self):
        assert (
            config(
                "set",
                "json",
                "true",
                "clean_tarballs",
                "true",
                "--file",
                TestConfigModifiers.home_rc_path,
            ).splitlines()
            == "Key is invalid or more than one key was received".splitlines()
        )

    def test_file_remove_single_input(self):
        config("set", "json", "true", "--file", TestConfigModifiers.home_rc_path)
        assert (
            config(
                "remove-key", "json", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == []
        )

    def test_file_remove_non_existent_key(self):
        assert (
            config(
                "remove-key", "json", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "Key is not present in file".splitlines()
        )

    def test_file_remove_invalid_key(self):
        assert (
            config(
                "remove-key", "^&*&^def", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "Key is not present in file".splitlines()
        )

    def test_file_remove_vector(self):
        config(
            "append", "channels", "flowers", "--file", TestConfigModifiers.home_rc_path
        )
        config("remove-key", "channels", "--file", TestConfigModifiers.home_rc_path)
        assert (
            config(
                "get", "channels", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "Key is not present in file".splitlines()
        )

    def test_file_remove_vector_value(self):
        config(
            "append", "channels", "totoro", "--file", TestConfigModifiers.home_rc_path
        )
        config("append", "channels", "haku", "--file", TestConfigModifiers.home_rc_path)
        config(
            "remove", "channels", "totoro", "--file", TestConfigModifiers.home_rc_path
        )
        assert config(
            "get", "channels", "--file", TestConfigModifiers.home_rc_path
        ).splitlines() == ["channels:", "  - haku"]

    # TODO: This behavior should be fixed "channels: []"
    def test_file_remove_vector_all_values(self):
        config("append", "channels", "haku", "--file", TestConfigModifiers.home_rc_path)
        config("remove", "channels", "haku", "--file", TestConfigModifiers.home_rc_path)
        assert config(
            "get", "channels", "--file", TestConfigModifiers.home_rc_path
        ).splitlines() == ["Key is not present in file"]

    def test_file_remove_vector_nonexistent_value(self):
        config("append", "channels", "haku", "--file", TestConfigModifiers.home_rc_path)
        assert (
            config(
                "remove",
                "channels",
                "chihiro",
                "--file",
                TestConfigModifiers.home_rc_path,
            ).splitlines()
            == "Key is not present in file".splitlines()
        )

    def test_file_remove_vector_multiple_values(self):
        config("append", "channels", "haku", "--file", TestConfigModifiers.home_rc_path)
        assert (
            config(
                "remove",
                "channels",
                "haku",
                "chihiro",
                "--file",
                TestConfigModifiers.home_rc_path,
            ).splitlines()
            == "Only one value can be removed at a time".splitlines()
        )

    def test_file_append_single_input(self):
        config(
            "append", "channels", "flowers", "--file", TestConfigModifiers.home_rc_path
        )
        assert config(
            "get", "channels", "--file", TestConfigModifiers.home_rc_path
        ).splitlines() == ["channels:", "  - flowers"]

    def test_file_append_multiple_inputs(self):
        with open(TestConfigModifiers.home_rc_path, "w") as f:
            f.write("channels:\n  - foo")

        config(
            "append",
            "channels",
            "condesc,mambesc",
            "--file",
            TestConfigModifiers.home_rc_path,
        )
        assert (
            config(
                "get", "channels", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "channels:\n  - foo\n  - condesc\n  - mambesc".splitlines()
        )

    def test_file_append_multiple_keys(self):
        with open(TestConfigModifiers.home_rc_path, "w") as f:
            f.write("channels:\n  - foo\ndefault_channels:\n  - bar")

        config(
            "append",
            "channels",
            "condesc,mambesc",
            "default_channels",
            "condescd,mambescd",
            "--file",
            TestConfigModifiers.home_rc_path,
        )
        assert (
            config(
                "get", "channels", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "channels:\n  - foo\n  - condesc\n  - mambesc".splitlines()
        )
        assert (
            config(
                "get", "default_channels", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "default_channels:\n  - bar\n  - condescd\n  - mambescd".splitlines()
        )

    def test_file_append_invalid_input(self):
        with pytest.raises(subprocess.CalledProcessError):
            config("append", "--file", TestConfigModifiers.home_rc_path)

        with pytest.raises(subprocess.CalledProcessError):
            config("append", "@#A321", "--file", TestConfigModifiers.home_rc_path)

        with pytest.raises(subprocess.CalledProcessError):
            config("append", "json", "true", "--file", TestConfigModifiers.home_rc_path)

        with pytest.raises(subprocess.CalledProcessError):
            config(
                "append",
                "channels",
                "foo,bar",
                "json",
                "true",
                "--file",
                TestConfigModifiers.home_rc_path,
            )

    def test_file_prepend_single_input(self):
        config(
            "prepend", "channels", "flowers", "--file", TestConfigModifiers.home_rc_path
        )
        assert config(
            "get", "channels", "--file", TestConfigModifiers.home_rc_path
        ).splitlines() == ["channels:", "  - flowers"]

    def test_file_prepend_multiple_inputs(self):
        with open(TestConfigModifiers.home_rc_path, "w") as f:
            f.write("channels:\n  - foo")

        config(
            "prepend",
            "channels",
            "condesc,mambesc",
            "--file",
            TestConfigModifiers.home_rc_path,
        )
        assert (
            config(
                "get", "channels", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "channels:\n  - condesc\n  - mambesc\n  - foo".splitlines()
        )

    def test_file_prepend_multiple_keys(self):
        with open(TestConfigModifiers.home_rc_path, "w") as f:
            f.write("channels:\n  - foo\ndefault_channels:\n  - bar")

        config(
            "prepend",
            "channels",
            "condesc,mambesc",
            "default_channels",
            "condescd,mambescd",
            "--file",
            TestConfigModifiers.home_rc_path,
        )
        assert (
            config(
                "get", "channels", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "channels:\n  - condesc\n  - mambesc\n  - foo".splitlines()
        )
        assert (
            config(
                "get", "default_channels", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "default_channels:\n  - condescd\n  - mambescd\n  - bar".splitlines()
        )

    def test_file_prepend_invalid_input(self):
        with pytest.raises(subprocess.CalledProcessError):
            config("prepend", "--file", TestConfigModifiers.home_rc_path)

        with pytest.raises(subprocess.CalledProcessError):
            config("prepend", "@#A321", "--file", TestConfigModifiers.home_rc_path)

        with pytest.raises(subprocess.CalledProcessError):
            config(
                "prepend", "json", "true", "--file", TestConfigModifiers.home_rc_path
            )

        with pytest.raises(subprocess.CalledProcessError):
            config(
                "prepend",
                "channels",
                "foo,bar",
                "json",
                "true",
                "--file",
                TestConfigModifiers.home_rc_path,
            )

    def test_file_append_and_prepend_inputs(self):
        config(
            "append", "channels", "flowers", "--file", TestConfigModifiers.home_rc_path
        )
        config(
            "prepend", "channels", "powers", "--file", TestConfigModifiers.home_rc_path
        )
        assert config(
            "get", "channels", "--file", TestConfigModifiers.home_rc_path
        ).splitlines() == ["channels:", "  - powers", "  - flowers"]

    def test_file_set_and_append_inputs(self):
        config(
            "set", "experimental", "true", "--file", TestConfigModifiers.home_rc_path
        )
        config(
            "append", "channels", "gandalf", "--file", TestConfigModifiers.home_rc_path
        )
        config(
            "append", "channels", "legolas", "--file", TestConfigModifiers.home_rc_path
        )
        assert (
            config(
                "get", "experimental", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "experimental: true".splitlines()
        )
        assert config(
            "get", "channels", "--file", TestConfigModifiers.home_rc_path
        ).splitlines() == ["channels:", "  - gandalf", "  - legolas"]

    def test_file_set_and_prepend_inputs(self):
        config(
            "set", "experimental", "false", "--file", TestConfigModifiers.home_rc_path
        )
        config(
            "prepend", "channels", "zelda", "--file", TestConfigModifiers.home_rc_path
        )
        config(
            "prepend", "channels", "link", "--file", TestConfigModifiers.home_rc_path
        )
        assert (
            config(
                "get", "experimental", "--file", TestConfigModifiers.home_rc_path
            ).splitlines()
            == "experimental: false".splitlines()
        )
        assert config(
            "get", "channels", "--file", TestConfigModifiers.home_rc_path
        ).splitlines() == ["channels:", "  - link", "  - zelda"]

    def test_flag_env_set(self):
        config("set", "experimental", "false", "--env")
        assert (
            config("get", "experimental", "--env").splitlines()
            == "experimental: false".splitlines()
        )

    def test_flag_env_file_remove_vector(self):
        config("prepend", "channels", "thinga-madjiga", "--env")
        config("remove-key", "channels", "--env")
        assert (
            config("get", "channels", "--env").splitlines()
            == "Key is not present in file".splitlines()
        )

    def test_flag_env_file_set_and_append_inputs(self):
        config("set", "local_repodata_ttl", "2", "--env")
        config("append", "channels", "finn", "--env")
        config("append", "channels", "jake", "--env")
        assert (
            config("get", "local_repodata_ttl", "--env").splitlines()
            == "local_repodata_ttl: 2".splitlines()
        )
        assert config("get", "channels", "--env").splitlines() == [
            "channels:",
            "  - finn",
            "  - jake",
        ]


class TestConfigExpandVars:
    @staticmethod
    def _roundtrip(rc_file_path, attr, config_expr):
        rc_file_path.write_text(f"{attr}: {config_expr}")
        conf = config("list", "--json", "--no-env", "--rc-file", rc_file_path)
        return conf[attr]

    @pytest.mark.parametrize("yaml_quote", ["", '"'])
    def test_expandvars_conda(
        self, monkeypatch, tmpdir_factory, rc_file_path, yaml_quote
    ):
        """
        Environment variables should be expanded in settings that have expandvars=True.

        Test copied from Conda.
        """

        def _expandvars(attr, config_expr, env_value):
            config_expr = config_expr.replace("'", yaml_quote)
            monkeypatch.setenv("TEST_VAR", env_value)
            return self._roundtrip(rc_file_path, attr, config_expr)

        ssl_verify = _expandvars("ssl_verify", "${TEST_VAR}", "yes")
        assert ssl_verify

        for attr, env_value in [
            # Not supported by Micromamba
            # ("client_ssl_cert", "foo"),
            # ("client_ssl_cert_key", "foo"),
            ("channel_alias", "http://foo"),
        ]:
            value = _expandvars(attr, "${TEST_VAR}", env_value)
            assert value == env_value

        for attr in [
            # Not supported by Micromamba
            # "migrated_custom_channels",
            # "proxy_servers",
        ]:
            value = _expandvars(attr, "{'x': '${TEST_VAR}'}", "foo")
            assert value == {"x": "foo"}

        for attr in [
            "channels",
            "default_channels",
            # Not supported by Micromamba
            # "whitelist_channels",
        ]:
            value = _expandvars(attr, "['${TEST_VAR}']", "foo")
            assert value == ["foo"]

        custom_channels = _expandvars(
            "custom_channels", "{'x': '${TEST_VAR}'}", "http://foo"
        )
        assert custom_channels["x"] == "http://foo"

        custom_multichannels = _expandvars(
            "custom_multichannels", "{'x': ['${TEST_VAR}']}", "http://foo"
        )
        assert len(custom_multichannels["x"]) == 1
        assert custom_multichannels["x"][0] == "http://foo"

        envs_dirs = _expandvars("envs_dirs", "['${TEST_VAR}']", "/foo")
        assert any("foo" in d for d in envs_dirs)

        pkgs_dirs = _expandvars("pkgs_dirs", "['${TEST_VAR}']", "/foo")
        assert any("foo" in d for d in pkgs_dirs)

    @pytest.mark.parametrize(
        "inp,outp",
        [
            ("$", "$"),
            ("$$", "$"),
            ("foo", "foo"),
            ("${foo}bar1", "barbar1"),
            ("$[foo]bar", "$[foo]bar"),
            ("$bar bar", "$bar bar"),
            ("$?bar", "$?bar"),
            ("${foo", "${foo"),
            ("${{foo}}2", "baz1}2"),
            ("$bar$bar", "$bar$bar"),
            # Not supported by Micromamba
            # ("$foo$foo", "barbar"),
            # ("$foo}bar", "bar}bar"),
            # ("$foo$$foo bar", "bar$foo bar"),
            # ("$foo bar", "bar bar"),
            # *(
            #     [
            #         ("%", "%"),
            #         ("foo", "foo"),
            #         ("$foo bar", "bar bar"),
            #         ("${foo}bar", "barbar"),
            #         ("$[foo]bar", "$[foo]bar"),
            #         ("$bar bar", "$bar bar"),
            #         ("$?bar", "$?bar"),
            #         ("$foo}bar", "bar}bar"),
            #         ("${foo", "${foo"),
            #         ("${{foo}}", "baz1}"),
            #         ("$foo$foo", "barbar"),
            #         ("$bar$bar", "$bar$bar"),
            #         ("%foo% bar", "bar bar"),
            #         ("%foo%bar", "barbar"),
            #         ("%foo%%foo%", "barbar"),
            #         ("%%foo%%foo%foo%", "%foo%foobar"),
            #         ("%?bar%", "%?bar%"),
            #         ("%foo%%bar", "bar%bar"),
            #         ("'%foo%'%bar", "'%foo%'%bar"),
            #         ("bar'%foo%", "bar'%foo%"),
            #         ("'$foo'$foo", "'$foo'bar"),
            #         ("'$foo$foo", "'$foo$foo"),
            #     ]
            #     if platform.system() == "Windows"
            #     else []
            # ),
        ],
    )
    @pytest.mark.parametrize("yaml_quote", ["", '"', "'"])
    def test_expandvars_cpython(self, monkeypatch, rc_file_path, inp, outp, yaml_quote):
        """Tests copied from CPython."""
        monkeypatch.setenv("foo", "bar", True)
        monkeypatch.setenv("{foo", "baz1", True)
        monkeypatch.setenv("{foo}", "baz2", True)
        assert outp == self._roundtrip(
            rc_file_path, "channel_alias", yaml_quote + inp + yaml_quote
        )

    @pytest.mark.parametrize(
        "inp,outp",
        [
            (
                'x", "y',
                [
                    "${x",
                    "y}",
                ],
            ),
            ("x\ny", ["${x y}"]),
        ],
    )
    def test_envsubst_yaml_mixup(self, monkeypatch, rc_file_path, inp, outp):
        assert self._roundtrip(rc_file_path, "channels", f'["${{{inp}}}"]') == outp

    def test_envsubst_empty_var(self, monkeypatch, rc_file_path):
        monkeypatch.setenv("foo", "", True)
        # Windows does not support empty environment variables
        expected = "${foo}" if platform.system() == "Windows" else ""
        assert self._roundtrip(rc_file_path, "channel_alias", "'${foo}'") == expected
