import subprocess

import pytest

from . import helpers


def run_umamba(*args):
    """Run micromamba with the given args and return (returncode, stdout, stderr).

    Uses subprocess directly (not helpers.subprocess_run) because these commands
    are expected to fail; we want to inspect the error message on stderr without
    raising.
    """
    umamba = helpers.get_umamba()
    p = subprocess.run(
        [umamba, *args],
        capture_output=True,
        text=True,
    )
    return p.returncode, p.stdout, p.stderr


# Typos that should produce a "Did you mean '<cmd>'?" suggestion.
SUGGESTION_CASES = [
    # Missing / extra / substituted character
    ("instal", "install"),
    ("intsall", "install"),
    ("lst", "list"),
    ("uninstal", "uninstall"),
    ("creat", "create"),
    ("remov", "remove"),
    ("confog", "config"),
    # Transposed characters
    ("lsit", "list"),
    ("inof", "info"),
    ("ifno", "info"),
    ("rnu", "run"),
    ("actiavte", "activate"),
    ("reqoquery", "repoquery"),
    # Truncated command
    ("activ", "activate"),
]


@pytest.mark.parametrize("typo,expected", SUGGESTION_CASES)
def test_typo_suggests_closest_command(typo, expected):
    rc, out, err = run_umamba(typo)

    assert rc != 0

    # The suggestion is emitted and names the closest command.
    assert f"Did you mean '{expected}'?" in err

    # The original CLI11 error is preserved.
    assert "not expected" in err

    # Ordering is conda-like: suggestion appears before the generic help hint.
    assert err.index(f"Did you mean '{expected}'?") < err.index("Run with --help")


def test_alias_is_matched():
    # 'uninstall' is registered with the alias 'remove';
    # a typo close to an alias should still be suggested.

    rc, out, err = run_umamba("uninstal", "xtensor")
    assert rc != 0
    assert "Did you mean 'uninstall'?" in err


def test_sub_subcommand_typo_is_matched():
    # Suggestions should work for nested subcommands too. 'whoneds' is a typo of
    # the 'repoquery whoneeds' subcommand.
    rc, out, err = run_umamba("repoquery", "whoneds", "openmp")
    assert rc != 0
    assert "Did you mean 'whoneeds'?" in err


# Inputs that must NOT produce a suggestion.
NO_SUGGESTION_CASES = [
    # A flag is not a subcommand typo; it must be ignored.
    (["list", "--badflag"],),
    # Nonsense below the difflib similarity cutoff (0.6) gets no suggestion.
    (["zzzzzzzz"],),
    (["xq"],),
    (["qwerty"],),
]


@pytest.mark.parametrize("args", [c[0] for c in NO_SUGGESTION_CASES])
def test_no_false_suggestion(args):
    rc, out, err = run_umamba(*args)
    assert "Did you mean" not in err


def test_valid_command_has_no_suggestion():
    # A correctly spelled command must never trigger the suggestion path.
    rc, out, err = run_umamba("--help")
    assert rc == 0
    assert "Did you mean" not in err
    assert "Did you mean" not in out
