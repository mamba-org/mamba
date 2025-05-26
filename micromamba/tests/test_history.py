import re

# Need to import everything to get fixtures
from .helpers import *  # noqa: F403
from . import helpers


def test_history(tmp_home, tmp_root_prefix):
    env_name = "myenv"
    helpers.create("-n", env_name, "python=3.8")
    helpers.install("-n", env_name, "xtl")
    helpers.uninstall("-n", env_name, "xtl")

    effective_prefix = tmp_root_prefix / "envs" / "myenv"
    history_path = effective_prefix / "conda-meta" / "history"
    assert (history_path).exists()
    with open(history_path) as f:
        history = f.read()
    assert len(re.findall(r"\+https:\/\/conda.anaconda.org\/conda-forge\/.+::xtl", history)) > 0
    assert len(re.findall(r"-https:\/\/conda.anaconda.org\/conda-forge\/.+::xtl", history)) > 0
