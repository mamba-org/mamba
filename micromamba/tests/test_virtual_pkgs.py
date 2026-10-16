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

    def test_virtual_cuda_arch(self):
        env = {**os.environ, "CONDA_OVERRIDE_CUDA": "12.0"}

        infos = info(env={**env, "CONDA_OVERRIDE_CUDA_ARCH": "8.0"})
        assert "__cuda=12.0=0" in infos
        assert "__cuda_arch=8.0=0" in infos

        infos = info(env={**env, "CONDA_OVERRIDE_CUDA_ARCH": ""})
        assert "__cuda=12.0=0" in infos
        assert "__cuda_arch" not in infos

        # `__cuda_arch` requires `__cuda`
        infos = info(env={**env, "CONDA_OVERRIDE_CUDA": "", "CONDA_OVERRIDE_CUDA_ARCH": "8.0"})
        assert "__cuda_arch" not in infos
