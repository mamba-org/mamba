import os
import platform
import shutil
import subprocess
import textwrap
from pathlib import Path

import pytest
import yaml

from . import helpers


class Dumper(yaml.Dumper):
    """A YAML dumper to properly indent lists.

    https://github.com/yaml/pyyaml/issues/234#issuecomment-765894586
    """

    def increase_indent(self, flow=False, *args, **kwargs):
        return super().increase_indent(flow=flow, indentless=False)


def config(*args):
    umamba = helpers.get_umamba()
    cmd = [umamba, "config"] + [arg for arg in args if arg]
    res = helpers.subprocess_run(*cmd)
    if "--json" in args:
        j = yaml.load(res, yaml.FullLoader)
        return j

    return res.decode()


@pytest.fixture
def rc_file_args(request):
    """Parametrizable fixture to choose content of rc file as a dict."""
    return getattr(request, "param", {})


@pytest.fixture
def rc_file_text(rc_file_args):
    """The content of the rc_file."""
    return yaml.dump(rc_file_args, Dumper=Dumper)


@pytest.fixture
def rc_file(
    request,
    rc_file_text,
    tmp_home,
    tmp_root_prefix,
    tmp_prefix,
    tmp_path,
    user_config_dir,
):
    """Parametrizable fixture to create an rc file at the desired location.

    The file is created in an isolated folder and set as the prefix, root prefix, or
    home folder.
    """
    if hasattr(request, "param"):
        where, rc_filename = request.param
        if where == "home":
            rc_file = tmp_home / rc_filename
        elif where == "root_prefix":
            rc_file = tmp_root_prefix / rc_filename
        elif where == "prefix":
            rc_file = tmp_prefix / rc_filename
        elif where == "user_config_dir":
            rc_file = user_config_dir / rc_filename
        elif where == "env_set_xdg":
            os.environ["XDG_CONFIG_HOME"] = str(tmp_home / "custom_xdg_config_dir")
            rc_file = tmp_home / "custom_xdg_config_dir" / "mamba" / rc_filename
        elif where == "absolute":
            rc_file = Path(rc_filename)
        else:
            raise ValueError("Bad rc file location")
        if rc_file.suffix == ".d":
            rc_file = rc_file / "test.yaml"
    else:
        rc_file = tmp_path / "umamba/config.yaml"

    rc_file.parent.mkdir(parents=True, exist_ok=True)
    with open(rc_file, "w+") as f:
        f.write(rc_file_text)

    return rc_file


class TestConfig:
    def test_config_empty(self, tmp_home):
        assert "Configuration of micromamba" in config()

    @pytest.mark.parametrize("quiet_flag", ["-q", "--quiet"])
    def test_config_quiet(self, quiet_flag, tmp_home):
        assert config(quiet_flag) == ""


class TestConfigSources:
    @pytest.mark.parametrize(
        "rc_file", (("home", "dummy.yaml"), ("home", ".mambarc")), indirect=True
    )
    @pytest.mark.parametrize(
        "rc_file_args", ({"override_channels_enabled": True},), indirect=True
    )
    @pytest.mark.parametrize("quiet_flag", ["-q", "--quiet"])
    @pytest.mark.parametrize("norc", [False, True])
    def test_config_sources(self, rc_file, quiet_flag, norc):
        if norc:
            with pytest.raises(subprocess.CalledProcessError):
                config("sources", quiet_flag, "--rc-file", rc_file, "--no-rc")
        else:
            res = config("sources", quiet_flag, "--rc-file", rc_file)
            rc_file_short = str(rc_file).replace(os.path.expanduser("~"), "~")
            assert res.strip().splitlines() == (
                f"Configuration files (by precedence order):\n{rc_file_short}".splitlines()
            )

    @pytest.mark.parametrize("quiet_flag", ["-q", "--quiet"])
    @pytest.mark.parametrize("norc", [False, True])
    def test_config_sources_empty(self, tmp_prefix, quiet_flag, norc):
        if norc:
            res = config("sources", quiet_flag, "--no-rc")
            assert res.strip() == "Configuration files disabled by --no-rc flag"
        else:
            res = config("sources", quiet_flag)
            assert res.startswith("Configuration files (by precedence order):")

    # TODO: test system located sources?
    @pytest.mark.parametrize(
        "rc_file",
        (
            # "/etc/conda/.condarc",
            # "/etc/conda/condarc",
            # "/etc/conda/condarc.d/",
            # "/etc/conda/.mambarc",
            # "/var/lib/conda/.condarc",
            # "/var/lib/conda/condarc",
            # "/var/lib/conda/condarc.d/",
            # "/var/lib/conda/.mambarc",
            ("user_config_dir", "mambarc"),
            ("env_set_xdg", "mambarc"),
            ("home", ".conda/.condarc"),
            ("home", ".conda/condarc"),
            ("home", ".conda/condarc.d"),
            ("home", ".condarc"),
            ("home", ".mambarc"),
            ("root_prefix", ".condarc"),
            ("root_prefix", "condarc"),
            ("root_prefix", "condarc.d"),
            ("root_prefix", ".mambarc"),
            ("prefix", ".condarc"),
            ("prefix", "condarc"),
            ("prefix", "condarc.d"),
            ("prefix", ".mambarc"),
        ),
        indirect=True,
    )
    @pytest.mark.parametrize(
        "rc_file_args", ({"override_channels_enabled": True},), indirect=True
    )
    def test_config_rc_file(self, rc_file, tmp_env_name):
        srcs = config("sources", "-n", tmp_env_name).strip().splitlines()
        short_name = str(rc_file).replace(os.path.expanduser("~"), "~")
        expected_srcs = (
            f"Configuration files (by precedence order):\n{short_name}".splitlines()
        )
        assert srcs == expected_srcs

    @pytest.mark.parametrize(
        "rc_file",
        [("home", "somefile.yml")],
        indirect=True,
    )
    @pytest.mark.parametrize(
        "rc_file_args", ({"override_channels_enabled": True},), indirect=True
    )
    def test_config_expand_user(self, rc_file):
        rc_file_short = str(rc_file).replace(os.path.expanduser("~"), "~")
        res = config("sources", "--rc-file", rc_file)
        assert (
            res.strip().splitlines()
            == f"Configuration files (by precedence order):\n{rc_file_short}".splitlines()
        )


class TestConfigList:
    @pytest.mark.parametrize("rc_file_args", ({"channels": ["channel1", "channel2"]},))
    def test_list_with_rc(self, rc_file, rc_file_text):
        assert (
            config("list", "--no-env", "--rc-file", rc_file).splitlines()
            == rc_file_text.splitlines()
        )

    def test_list_without_rc(self):
        assert config("list", "--no-env", "--no-rc").splitlines()[:-1] == []

    @pytest.mark.parametrize("source_flag", ["--sources", "-s"])
    @pytest.mark.parametrize("rc_file_args", ({"channels": ["channel1", "channel2"]},))
    def test_list_with_sources(self, rc_file, source_flag):
        home_folder = os.path.expanduser("~")
        src = f"  # '{str(rc_file).replace(home_folder, '~')}'"
        assert (
            config("list", "--no-env", "--rc-file", rc_file, source_flag).splitlines()
            == f"channels:\n  - channel1{src}\n  - channel2{src}\n".splitlines()
        )

    @pytest.mark.parametrize("source_flag", ["--sources", "-s"])
    @pytest.mark.parametrize("rc_file_args", ({"custom_channels": {"key1": "value1"}},))
    def test_list_map_with_sources(self, rc_file, source_flag):
        home_folder = os.path.expanduser("~")
        src = f"  # '{str(rc_file).replace(home_folder, '~')}'"
        assert (
            config("list", "--no-env", "--rc-file", rc_file, source_flag).splitlines()
            == f"custom_channels:\n  key1: value1{src}\n".splitlines()
        )

    @pytest.mark.parametrize("desc_flag", ["--descriptions", "-d"])
    @pytest.mark.parametrize("rc_file_args", ({"channels": ["channel1", "channel2"]},))
    def test_list_with_descriptions(self, rc_file, desc_flag):
        assert (
            config("list", "--no-env", "--rc-file", rc_file, desc_flag).splitlines()
            == "# channels\n#   Define the list of channels\nchannels:\n"
            "  - channel1\n  - channel2\n".splitlines()
        )

    @pytest.mark.parametrize("desc_flag", ["--long-descriptions", "-l"])
    @pytest.mark.parametrize("rc_file_args", ({"channels": ["channel1", "channel2"]},))
    def test_list_with_long_descriptions(self, rc_file, desc_flag):
        assert (
            config("list", "--no-env", "--rc-file", rc_file, desc_flag).splitlines()
            == "# channels\n#   The list of channels where the packages will be searched for.\n"
            "#   See also 'channel_priority'.\nchannels:\n  - channel1\n  - channel2\n".splitlines()
        )

    @pytest.mark.parametrize("group_flag", ["--groups", "-g"])
    @pytest.mark.parametrize("rc_file_args", ({"channels": ["channel1", "channel2"]},))
    def test_list_with_groups(self, rc_file, group_flag):
        group = (
            "# ######################################################\n"
            "# #               Channels Configuration               #\n"
            "# ######################################################\n\n"
        )

        assert (
            config(
                "list", "--no-env", "--rc-file", rc_file, "-d", group_flag
            ).splitlines()
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
        rc_dir = os.path.expanduser(
            os.path.join("~", "test_mamba", helpers.random_string())
        )
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
                == "offline: false  # 'MAMBA_OFFLINE'".splitlines()
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
    def test_file_set_single_input(self, rc_file):
        config("set", "json", "true", "--file", rc_file)
        assert (
            config("get", "json", "--file", rc_file).splitlines()
            == "json: true".splitlines()
        )

    def test_file_set_change_key_value(self, rc_file):
        config("set", "json", "true", "--file", rc_file)
        config("set", "json", "false", "--file", rc_file)
        assert (
            config("get", "json", "--file", rc_file).splitlines()
            == "json: false".splitlines()
        )

    def test_file_set_invalit_input(self, rc_file):
        assert (
            config("set", "$%#@abc", "--file", rc_file).splitlines()
            == "Key is invalid or more than one key was received".splitlines()
        )

    def test_file_set_multiple_inputs(self, rc_file):
        assert (
            config(
                "set",
                "json",
                "true",
                "clean_tarballs",
                "true",
                "--file",
                rc_file,
            ).splitlines()
            == "Key is invalid or more than one key was received".splitlines()
        )

    def test_file_remove_single_input(self, rc_file):
        config("set", "json", "true", "--file", rc_file)
        assert config("remove-key", "json", "--file", rc_file).splitlines() == []

    def test_file_remove_non_existent_key(self, rc_file):
        assert (
            config("remove-key", "json", "--file", rc_file).splitlines()
            == "Key is not present in file".splitlines()
        )

    def test_file_remove_invalid_key(self, rc_file):
        assert (
            config("remove-key", "^&*&^def", "--file", rc_file).splitlines()
            == "Key is not present in file".splitlines()
        )

    def test_file_remove_vector(self, rc_file):
        config("append", "channels", "flowers", "--file", rc_file)
        config("remove-key", "channels", "--file", rc_file)
        assert (
            config("get", "channels", "--file", rc_file).splitlines()
            == "Key is not present in file".splitlines()
        )

    def test_file_remove_vector_value(self, rc_file):
        # Backward test compatibility: when an empty file exists, the formatting is different
        rc_file.unlink()
        config("append", "channels", "totoro", "--file", rc_file)
        config("append", "channels", "haku", "--file", rc_file)
        config("remove", "channels", "totoro", "--file", rc_file)
        assert config("get", "channels", "--file", rc_file).splitlines() == [
            "channels:",
            "  - haku",
        ]

    # TODO: This behavior should be fixed "channels: []"
    def test_file_remove_vector_all_values(self, rc_file):
        config("append", "channels", "haku", "--file", rc_file)
        config("remove", "channels", "haku", "--file", rc_file)
        assert config("get", "channels", "--file", rc_file).splitlines() == [
            "Key is not present in file"
        ]

    def test_file_remove_vector_nonexistent_value(self, rc_file):
        config("append", "channels", "haku", "--file", rc_file)
        assert (
            config(
                "remove",
                "channels",
                "chihiro",
                "--file",
                rc_file,
            ).splitlines()
            == "Key is not present in file".splitlines()
        )

    def test_file_remove_vector_multiple_values(self, rc_file):
        config("append", "channels", "haku", "--file", rc_file)
        assert (
            config(
                "remove",
                "channels",
                "haku",
                "chihiro",
                "--file",
                rc_file,
            ).splitlines()
            == "Only one value can be removed at a time".splitlines()
        )

    def test_file_append_single_input(self, rc_file):
        # Backward test compatibility: when an empty file exists, the formatting is different
        rc_file.unlink()
        config("append", "channels", "flowers", "--file", rc_file)
        assert config("get", "channels", "--file", rc_file).splitlines() == [
            "channels:",
            "  - flowers",
        ]

    def test_file_append_multiple_inputs(self, rc_file):
        with open(rc_file, "w") as f:
            f.write("channels:\n  - foo")

        config(
            "append",
            "channels",
            "condesc,mambesc",
            "--file",
            rc_file,
        )
        assert (
            config("get", "channels", "--file", rc_file).splitlines()
            == "channels:\n  - foo\n  - condesc\n  - mambesc".splitlines()
        )

    def test_file_append_multiple_keys(self, rc_file):
        with open(rc_file, "w") as f:
            f.write("channels:\n  - foo\ndefault_channels:\n  - bar")

        config(
            "append",
            "channels",
            "condesc,mambesc",
            "default_channels",
            "condescd,mambescd",
            "--file",
            rc_file,
        )
        assert (
            config("get", "channels", "--file", rc_file).splitlines()
            == "channels:\n  - foo\n  - condesc\n  - mambesc".splitlines()
        )
        assert (
            config("get", "default_channels", "--file", rc_file).splitlines()
            == "default_channels:\n  - bar\n  - condescd\n  - mambescd".splitlines()
        )

    def test_file_append_invalid_input(self, rc_file):
        with pytest.raises(subprocess.CalledProcessError):
            config("append", "--file", rc_file)

        with pytest.raises(subprocess.CalledProcessError):
            config("append", "@#A321", "--file", rc_file)

        with pytest.raises(subprocess.CalledProcessError):
            config("append", "json", "true", "--file", rc_file)

        with pytest.raises(subprocess.CalledProcessError):
            config(
                "append",
                "channels",
                "foo,bar",
                "json",
                "true",
                "--file",
                rc_file,
            )

    def test_file_prepend_single_input(self, rc_file):
        # Backward test compatibility: when an empty file exists, the formatting is different
        rc_file.unlink()
        config("prepend", "channels", "flowers", "--file", rc_file)
        assert config("get", "channels", "--file", rc_file).splitlines() == [
            "channels:",
            "  - flowers",
        ]

    def test_file_prepend_multiple_inputs(self, rc_file):
        with open(rc_file, "w") as f:
            f.write("channels:\n  - foo")

        config(
            "prepend",
            "channels",
            "condesc,mambesc",
            "--file",
            rc_file,
        )
        assert (
            config("get", "channels", "--file", rc_file).splitlines()
            == "channels:\n  - condesc\n  - mambesc\n  - foo".splitlines()
        )

    def test_file_prepend_multiple_keys(self, rc_file):
        with open(rc_file, "w") as f:
            f.write("channels:\n  - foo\ndefault_channels:\n  - bar")

        config(
            "prepend",
            "channels",
            "condesc,mambesc",
            "default_channels",
            "condescd,mambescd",
            "--file",
            rc_file,
        )
        assert (
            config("get", "channels", "--file", rc_file).splitlines()
            == "channels:\n  - condesc\n  - mambesc\n  - foo".splitlines()
        )
        assert (
            config("get", "default_channels", "--file", rc_file).splitlines()
            == "default_channels:\n  - condescd\n  - mambescd\n  - bar".splitlines()
        )

    def test_file_prepend_invalid_input(self, rc_file):
        with pytest.raises(subprocess.CalledProcessError):
            config("prepend", "--file", rc_file)

        with pytest.raises(subprocess.CalledProcessError):
            config("prepend", "@#A321", "--file", rc_file)

        with pytest.raises(subprocess.CalledProcessError):
            config("prepend", "json", "true", "--file", rc_file)

        with pytest.raises(subprocess.CalledProcessError):
            config(
                "prepend",
                "channels",
                "foo,bar",
                "json",
                "true",
                "--file",
                rc_file,
            )

    def test_file_append_and_prepend_inputs(self, rc_file):
        # Backward test compatibility: when an empty file exists, the formatting is different
        rc_file.unlink()
        config("append", "channels", "flowers", "--file", rc_file)
        config("prepend", "channels", "powers", "--file", rc_file)
        assert config("get", "channels", "--file", rc_file).splitlines() == [
            "channels:",
            "  - powers",
            "  - flowers",
        ]

    def test_file_set_and_append_inputs(self, rc_file):
        # Backward test compatibility: when an empty file exists, the formatting is different
        rc_file.unlink()
        config("set", "experimental", "true", "--file", rc_file)
        config("append", "channels", "gandalf", "--file", rc_file)
        config("append", "channels", "legolas", "--file", rc_file)
        assert (
            config("get", "experimental", "--file", rc_file).splitlines()
            == "experimental: true".splitlines()
        )
        assert config("get", "channels", "--file", rc_file).splitlines() == [
            "channels:",
            "  - gandalf",
            "  - legolas",
        ]

    def test_file_set_and_prepend_inputs(self, rc_file):
        # Backward test compatibility: when an empty file exists, the formatting is different
        rc_file.unlink()
        config("set", "experimental", "false", "--file", rc_file)
        config("prepend", "channels", "zelda", "--file", rc_file)
        config("prepend", "channels", "link", "--file", rc_file)
        assert (
            config("get", "experimental", "--file", rc_file).splitlines()
            == "experimental: false".splitlines()
        )
        assert config("get", "channels", "--file", rc_file).splitlines() == [
            "channels:",
            "  - link",
            "  - zelda",
        ]

    def test_flag_env_set(self, rc_file):
        config("set", "experimental", "false", "--env")
        assert (
            config("get", "experimental", "--env").splitlines()
            == "experimental: false".splitlines()
        )

    def test_flag_env_file_remove_vector(self, rc_file):
        config("prepend", "channels", "thinga-madjiga", "--env")
        config("remove-key", "channels", "--env")
        assert (
            config("get", "channels", "--env").splitlines()
            == "Key is not present in file".splitlines()
        )

    def test_flag_env_file_set_and_append_inputs(self, rc_file):
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
    def _roundtrip(rc_file_path, rc_contents):
        rc_file_path.write_text(rc_contents)
        return config("list", "--json", "--no-env", "--rc-file", rc_file_path)

    @classmethod
    def _roundtrip_attr(cls, rc_file_path, attr, config_expr):
        return cls._roundtrip(rc_file_path, f"{attr}: {config_expr}")[attr]

    @pytest.mark.parametrize("yaml_quote", ["", '"'])
    def test_expandvars_conda(self, monkeypatch, tmpdir_factory, rc_file, yaml_quote):
        """
        Environment variables should be expanded in settings that have expandvars=True.

        Test copied from Conda.
        """

        def _expandvars(attr, config_expr, env_value):
            config_expr = config_expr.replace("'", yaml_quote)
            monkeypatch.setenv("TEST_VAR", env_value)
            return self._roundtrip_attr(rc_file, attr, config_expr)

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
            # Tests copied from: cpython/Lib/test/test_genericpath.py
            ("$", "$"),
            ("$$", "$$"),
            ("foo", "foo"),
            ("${foo}bar1", "barbar1"),
            ("$[foo]bar", "$[foo]bar"),
            ("$bar bar", "$bar bar"),
            ("$?bar", "$?bar"),
            ("$foo}bar", "bar}bar"),
            ("${foo", "${foo"),
            # Not supported by Micromamba
            # ("${{foo}}", "baz1"),
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
            # Our tests:
            ("$bar$bar", "$bar$bar"),
            ("$foo$foo", "barbar"),
            ("$foo$$foo bar", "bar$bar bar"),
            ("$foo bar", "bar bar"),
        ],
    )
    @pytest.mark.parametrize("yaml_quote", ["", '"', "'"])
    def test_expandvars_cpython(self, monkeypatch, rc_file, inp, outp, yaml_quote):
        monkeypatch.setenv("foo", "bar", True)
        monkeypatch.setenv("{foo", "baz1", True)
        monkeypatch.setenv("{foo}", "baz2", True)
        assert outp == self._roundtrip_attr(
            rc_file, "channel_alias", yaml_quote + inp + yaml_quote
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
    def test_envsubst_yaml_mixup(self, monkeypatch, rc_file, inp, outp):
        assert self._roundtrip_attr(rc_file, "channels", f'["${{{inp}}}"]') == outp

    def test_envsubst_empty_var(self, monkeypatch, rc_file):
        monkeypatch.setenv("foo", "", True)
        # Windows does not support empty environment variables
        expected = "${foo}" if platform.system() == "Windows" else ""
        assert self._roundtrip_attr(rc_file, "channel_alias", "'${foo}'") == expected

    def test_envsubst_windows_problem(self, monkeypatch, rc_file):
        # Real-world problematic .condarc file
        condarc = textwrap.dedent(
            """
            channel_alias: https://xxxxxxxxxxxxxxxxxxxx.com/t/${CONDA_API_KEY}/get

            channels:
              - xxxxxxxxxxx
              - yyyyyyyyyyyy
              - conda-forge

            custom_channels:
              yyyyyyyyyyyy: https://${CONDA_CHANNEL_UPLOAD_USER}:${CONDA_CHANNEL_UPLOAD_PASSWORD}@xxxxxxxxxxxxxxx.com

            custom_multichannels:
              conda-forge:
                - https://conda.anaconda.org/conda-forge
            """
        )
        monkeypatch.setenv("CONDA_API_KEY", "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk", True)
        monkeypatch.setenv("CONDA_CHANNEL_UPLOAD_USER", "uuuuuuuuu", True)
        monkeypatch.setenv(
            "CONDA_CHANNEL_UPLOAD_PASSWORD", "pppppppppppppppppppp", True
        )
        out = self._roundtrip(rc_file, condarc)
        assert (
            out["channel_alias"]
            == "https://xxxxxxxxxxxxxxxxxxxx.com/t/kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk/get"
        )
        assert out["custom_channels"] == {
            "yyyyyyyyyyyy": "https://uuuuuuuuu:pppppppppppppppppppp@xxxxxxxxxxxxxxx.com"
        }
