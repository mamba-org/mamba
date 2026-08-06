import pytest

from . import helpers


def test_completer_lists_top_level_commands(tmp_home, tmp_root_prefix):
    """Regression: duplicate deactivate must not abort completer (CLI::OptionAlreadyAdded)."""
    umamba = helpers.get_umamba()
    out = helpers.subprocess_run(umamba, "completer", "").decode()

    for command in (
        "activate",
        "deactivate",
        "create",
        "install",
        "env",
        "shell",
        "update",
        "remove",
        "list",
        "info",
    ):
        assert command in out


@pytest.mark.parametrize(
    "prefix, expected",
    [
        ("a", ("activate", "auth")),
        ("de", ("deactivate",)),
        ("in", ("info", "install")),
        ("rem", ("remove",)),
        ("sh", ("shell",)),
    ],
)
def test_completer_prefix_matches_commands(tmp_home, tmp_root_prefix, prefix, expected):
    umamba = helpers.get_umamba()
    out = helpers.subprocess_run(umamba, "completer", prefix).decode()
    for command in expected:
        assert command in out


def test_completer_activate_lists_env_names(tmp_home, tmp_root_prefix, tmp_empty_env, tmp_env_name):
    umamba = helpers.get_umamba()
    out = helpers.subprocess_run(umamba, "completer", "-n", "").decode()
    assert tmp_env_name in out


def test_completer_shell_lists_subcommands(tmp_home, tmp_root_prefix):
    umamba = helpers.get_umamba()
    out = helpers.subprocess_run(umamba, "completer", "shell", "").decode()
    for command in ("activate", "deactivate", "hook", "init"):
        assert command in out


def test_completer_install_option_prefix(tmp_home, tmp_root_prefix):
    umamba = helpers.get_umamba()
    out = helpers.subprocess_run(umamba, "completer", "install", "--c").decode()
    assert "--channel" in out
