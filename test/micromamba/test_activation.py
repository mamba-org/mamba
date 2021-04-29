import os
import platform
import shutil
import subprocess
import sys

import pytest

from .helpers import *

plat, running_os = None, None

if platform.system() == "Linux":
    plat = "linux"
    running_os = "unix"
elif platform.system() == "Darwin":
    plat = "osx"
    running_os = "unix"
elif platform.system() == "Windows":
    plat = "win"
    running_os = "win"


suffixes = {
    "cmdexe": ".bat",
    "bash": ".sh",
    "zsh": ".sh",
    "xonsh": ".sh",
    "powershell": ".ps1",
}

paths = {
    "osx": {"zsh": "~/.zshrc", "bash": "~/.bash_profile"},
    "linux": {"zsh": "~/.zshrc", "bash": "~/.bashrc"},
}


def write_script(interpreter, lines, path):
    fname = os.path.join(path, "script" + suffixes[interpreter])
    with open(fname, "w") as fo:
        fo.write("\n".join(lines) + "\n")

    return fname


def emit_check(cond):
    return cmds.if_(cond).then_(cmds.echo("YES")).else_(cmds.echo("NOPE"))


enable_on_os = {
    "win": {"powershell", "cmdexe"},
    # 'unix': {'bash', 'zsh', 'xonsh'},
    "unix": {"bash", "zsh"},
}

shell_files = [
    Path(x).expanduser()
    for x in ["~/.bashrc", "~/.bash_profile", "~/.zshrc", "~/.zsh_profile"]
]


@pytest.fixture
def clean_shell_files():

    for f in shell_files:
        if f.exists():
            f.rename(str(f) + ".mamba_test_backup")
            f.touch()

    yield shell_files

    for f in shell_files:
        f_bkup = Path(str(f) + ".mamba_test_backup")
        if f_bkup.exists():
            f_bkup.rename(str(f_bkup)[: -len(".mamba_test_backup")])


def call_interpreter(s, tmp_path, interpreter, interactive=False, env=None):
    f = write_script(interpreter, s, tmp_path)
    if interpreter not in enable_on_os[running_os]:
        return None, None

    if interpreter == "cmdexe":
        args = ["cmd.exe", "/C", f]
    elif interpreter == "powershell":
        args = ["powershell.exe", "-File", f]
    else:
        args = [interpreter, f]
        if interactive:
            args.insert(1, "-i")
        if interactive and interpreter == "bash":
            args.insert(1, "--login")

    res = subprocess.run(
        args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True, env=env
    )
    stdout = res.stdout.decode("utf-8").strip()
    stderr = res.stderr.decode("utf-8").strip()
    return stdout, stderr


def get_interpreters(exclude=[]):
    return [x for x in enable_on_os[running_os] if x not in exclude]


class TestActivation:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    other_root_prefix = os.path.expanduser(
        os.path.join("~", "tmproot" + random_string())
    )
    prefix = os.path.join(root_prefix, "envs", env_name)

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = cls.root_prefix
        os.environ["CONDA_PREFIX"] = cls.prefix

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = cls.current_root_prefix
        os.environ["CONDA_PREFIX"] = cls.current_prefix
        for f in shell_files:
            f_bkup = Path(str(f) + ".mamba_test_backup")
            if f_bkup.exists():
                f_bkup.rename(str(f_bkup)[: -len(".mamba_test_backup")])

        if os.path.exists(cls.root_prefix):
            shutil.rmtree(cls.root_prefix)
        if os.path.exists(cls.other_root_prefix):
            shutil.rmtree(cls.other_root_prefix)

    @pytest.mark.parametrize("interpreter", get_interpreters())
    def test_hello_world(self, tmp_path, interpreter):
        s = ["echo 'hello world'"]
        stdout, stderr = call_interpreter(s, tmp_path, interpreter)
        assert stdout == "hello world"

    @pytest.mark.parametrize("interpreter", get_interpreters())
    def test_shell_init(self, tmp_path, interpreter, clean_shell_files):
        cwd = os.getcwd()
        umamba = get_umamba(cwd=cwd)
        env = {"MAMBA_ROOT_PREFIX": self.root_prefix}
        call = lambda s: call_interpreter(s, tmp_path, interpreter, env=env)
        s = ["echo $MAMBA_ROOT_PREFIX"]
        stdout, stderr = call(s)
        assert stdout == self.root_prefix

        # TODO remove root prefix here
        s = [f"{umamba} shell init -p $MAMBA_ROOT_PREFIX"]
        stdout, stderr = call(s)
        global plat
        path = Path(paths[plat][interpreter]).expanduser()
        with open(path) as fi:
            x = fi.read()
            assert "mamba" in x
            prev_text = x

        s = [f"{umamba} shell init -p $MAMBA_ROOT_PREFIX"]
        stdout, stderr = call(s)

        with open(path) as fi:
            x = fi.read()
            assert "mamba" in x
            assert prev_text == x

        with open(path) as fi:
            prevlines = fi.readlines()

        with open(path, "w") as fo:
            text = "\n".join(
                ["", "", "echo 'hihi'", ""]
                + [x.rstrip("\n\r") for x in prevlines]
                + ["", "", "echo 'hehe'"]
            )
            fo.write(text)

        s = [f"{umamba} shell init -p $MAMBA_ROOT_PREFIX"]
        stdout, stderr = call(s)
        with open(path) as fi:
            x = fi.read()
            assert "mamba" in x
            assert text == x

        s = [f"{umamba} shell init -p {self.other_root_prefix}"]
        stdout, stderr = call(s)

        with open(path) as fi:
            x = fi.read()
            assert "mamba" in x
            assert self.other_root_prefix in x
            assert self.root_prefix not in x

    @pytest.mark.parametrize("interpreter", get_interpreters())
    def test_activation(self, tmp_path, interpreter, clean_shell_files):
        cwd = os.getcwd()
        umamba = get_umamba(cwd=cwd)

        s = [f"{umamba} shell init -p {self.root_prefix}"]
        stdout, stderr = call_interpreter(s, tmp_path, interpreter, env={})

        call = lambda s: call_interpreter(
            s, tmp_path, interpreter, interactive=True, env={}
        )

        if interpreter in ["bash", "zsh"]:
            extract_vars = lambda vxs: [f"echo {v}=${v}" for v in vxs]
        elif interpreter in ["cmdexe"]:
            extract_vars = lambda vxs: [f"echo {v}=%{v}%" for v in vxs]
        elif interpreter in ["powershell"]:
            extract_vars = lambda vxs: [f"echo {v}=$Env:{v}%" for v in vxs]

        def to_dict(out):
            return {k: v for k, v in [x.split("=", 1) for x in out.splitlines()]}

        rp = Path(self.root_prefix)
        if interpreter in ["bash", "zsh"]:
            evars = extract_vars(["CONDA_PREFIX", "CONDA_SHLVL", "PATH"])
            s = [f"micromamba activate"] + evars
            stdout, stderr = call(s)
            res = to_dict(stdout)
            assert "condabin" in res["PATH"]
            assert self.root_prefix in res["PATH"]
            assert f"CONDA_PREFIX={self.root_prefix}" in stdout.splitlines()
            assert f"CONDA_SHLVL=1" in stdout.splitlines()

            # throw with non-existent
            s = [f"micromamba activate nonexistent"]
            with pytest.raises(subprocess.CalledProcessError):
                stdout, stderr = call(s)

            # throw with non-existent
            s1 = [f"micromamba create -n abc"]
            s2 = [f"micromamba create -n xyz"]
            call(s1)
            call(s2)

            s = [
                f"micromamba activate",
                f"micromamba activate abc",
                f"micromamba activate xyz",
            ] + evars
            stdout, stderr = call(s)
            res = to_dict(stdout)
            assert str(rp / "condabin") in res["PATH"]
            assert str(rp / "bin") not in res["PATH"]
            assert str(rp / "envs" / "xyz") in res["PATH"]
            assert str(rp / "envs" / "abc") not in res["PATH"]

            s = [
                f"micromamba activate",
                f"micromamba activate abc",
                f"micromamba activate xyz --stack",
            ] + evars
            stdout, stderr = call(s)
            res = to_dict(stdout)
            assert str(rp / "condabin") in res["PATH"]
            assert str(rp / "bin") not in res["PATH"]
            assert str(rp / "envs" / "xyz" / "bin") in res["PATH"]
            assert str(rp / "envs" / "abc" / "bin") in res["PATH"]

            s = [
                f"micromamba activate",
                f"micromamba activate abc",
                f"micromamba activate --stack xyz",
            ] + evars
            stdout, stderr = call(s)
            res = to_dict(stdout)
            assert str(rp / "condabin") in res["PATH"]
            assert str(rp / "bin") not in res["PATH"]
            assert str(rp / "envs" / "xyz" / "bin") in res["PATH"]
            assert str(rp / "envs" / "abc" / "bin") in res["PATH"]

        elif interpreter == "cmdexe":
            s = [f"micromamba activate", "echo CONDA_PREFIX=%CONDA_PREFIX%"]
            stdout, stderr = call(s)
            assert f"CONDA_PREFIX={self.root_prefix}" in stdout.splitlines()
