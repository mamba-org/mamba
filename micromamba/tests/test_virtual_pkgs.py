import os
import platform

from .helpers import info


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
