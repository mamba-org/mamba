import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

import pytest
import yaml

from .helpers import info


def _conda_executable() -> str | None:
    found = shutil.which("conda")
    if found:
        return found
    name = "conda.exe" if os.name == "nt" else "conda"
    candidate = Path(sys.executable).resolve().parent / name
    return str(candidate) if candidate.is_file() else None


CONDA_EXE = _conda_executable()


def _env_without_host_overrides(**overrides: str) -> dict[str, str]:
    env = {key: value for key, value in os.environ.items() if not key.startswith("CONDA_OVERRIDE_")}
    env.update(overrides)
    return env


def _mamba_virtual_packages(rc_file, env: dict[str, str]) -> dict[str, tuple[str, str]]:
    data = info("--json", "--rc-file", str(rc_file), env=env)
    pkgs: dict[str, tuple[str, str]] = {}
    for item in data["virtual packages"]:
        name, version, build = item.split("=", 2)
        pkgs[name] = (version, build)
    return pkgs


def _conda_virtual_packages(rc_file, env: dict[str, str]) -> dict[str, tuple[str, str]]:
    run_env = dict(env)
    run_env["CONDARC"] = str(rc_file)
    proc = subprocess.run(
        [CONDA_EXE, "info", "--json"],
        capture_output=True,
        check=False,
        env=run_env,
    )
    assert proc.returncode == 0, f"conda info failed ({proc.returncode}): {proc.stderr.decode()}"
    data = json.loads(proc.stdout)
    return {name: (version, build) for name, version, build in data["virtual_pkgs"]}


class TestVirtualPkgs:
    def test_virtual_packages(self):
        infos = info()

        assert "virtual packages :" in infos
        assert "__archspec=1=" in infos
        if platform.system() == "Windows":
            assert "__win" in infos
        elif platform.system() == "Darwin":
            assert "__unix=0=0" in infos
            assert "__osx" in infos
        elif platform.system() == "Linux":
            assert "__unix=0=0" in infos
            assert "__glibc" in infos
            linux_ver = platform.release().split("-", 1)[0]
            assert f"__linux={linux_ver}=0" in infos

    def test_virtual_linux(self):
        if platform.system() == "Linux":
            infos = info()
            assert "__linux=" in infos
            assert "__linux=0=0" not in infos
        else:
            infos = info(env={**os.environ, "CONDA_SUBDIR": "linux-64"})
            assert "__linux=0=0" in infos

    def test_override_virtual_packages_from_rc(self, tmp_home, tmp_root_prefix, tmp_path):
        rc_file = tmp_path / ".mambarc"
        rc_file.write_text(
            """\
channels:
  - conda-forge
override_virtual_packages:
  cuda: "13.1"
  glibc: "2.15"
  archspec: "x86_64_v4"
"""
        )
        infos = info("--rc-file", str(rc_file))

        assert "__cuda=13.1=0" in infos
        assert "__archspec=1=x86_64_v4" in infos
        if platform.system() == "Linux":
            assert "__glibc=2.15=0" in infos

    def test_override_virtual_packages_dunder_keys(self, tmp_home, tmp_root_prefix, tmp_path):
        rc_file = tmp_path / ".mambarc"
        rc_file.write_text(
            """\
channels:
  - conda-forge
override_virtual_packages:
  __cuda: "11.8"
  __archspec: "x86_64_v2"
"""
        )
        infos = info("--rc-file", str(rc_file))

        assert "__cuda=11.8=0" in infos
        assert "__archspec=1=x86_64_v2" in infos

    def test_override_virtual_packages_env_over_rc(self, tmp_home, tmp_root_prefix, tmp_path):
        rc_file = tmp_path / ".mambarc"
        rc_file.write_text(
            """\
channels:
  - conda-forge
override_virtual_packages:
  cuda: "13.1"
"""
        )
        infos = info(
            "--rc-file",
            str(rc_file),
            env={**os.environ, "CONDA_OVERRIDE_CUDA": "9.0"},
        )

        assert "__cuda=9.0=0" in infos
        assert "__cuda=13.1=0" not in infos

    def test_override_virtual_packages_empty_env_suppresses_cuda(
        self, tmp_home, tmp_root_prefix, tmp_path
    ):
        rc_file = tmp_path / ".mambarc"
        rc_file.write_text(
            """\
channels:
  - conda-forge
override_virtual_packages:
  cuda: "13.1"
"""
        )
        infos = info(
            "--rc-file",
            str(rc_file),
            env={**os.environ, "CONDA_OVERRIDE_CUDA": ""},
        )

        assert "__cuda=" not in infos


@pytest.mark.skipif(CONDA_EXE is None, reason="conda is not available on PATH")
class TestOverrideVirtualPackagesMatchConda:
    @pytest.mark.parametrize(
        "rc_overrides, env_overrides, names",
        [
            pytest.param(
                {"cuda": "13.1", "glibc": "2.15", "archspec": "x86_64_v4"},
                {},
                ("__cuda", "__glibc", "__archspec"),
                id="plain-keys",
            ),
            pytest.param(
                {"__cuda": "11.8", "__archspec": "x86_64_v2"},
                {},
                ("__cuda", "__archspec"),
                id="dunder-keys",
            ),
            pytest.param(
                {"cuda": "13.1"},
                {"CONDA_OVERRIDE_CUDA": "9.0"},
                ("__cuda",),
                id="env-over-rc",
            ),
            pytest.param(
                {"cuda": "13.1"},
                {"CONDA_OVERRIDE_CUDA": ""},
                ("__cuda",),
                id="empty-env",
            ),
        ],
    )
    def test_override_matches_conda(
        self,
        tmp_home,
        tmp_root_prefix,
        tmp_path,
        rc_overrides,
        env_overrides,
        names,
    ):
        rc_file = tmp_path / "condarc"
        rc_file.write_text(
            yaml.dump(
                {
                    "channels": ["conda-forge"],
                    "override_virtual_packages": rc_overrides,
                }
            )
        )
        env = _env_without_host_overrides(**env_overrides)

        conda_pkgs = _conda_virtual_packages(rc_file, env)
        mamba_pkgs = _mamba_virtual_packages(rc_file, env)

        for name in names:
            assert conda_pkgs.get(name) == mamba_pkgs.get(name), (
                f"{name}: conda={conda_pkgs.get(name)} mamba={mamba_pkgs.get(name)}"
            )
