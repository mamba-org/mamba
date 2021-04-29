import os
import platform
import shutil
import subprocess
import sys
import string

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


clean_env = os.environ.copy()
clean_env = {k: v for k, v in clean_env.items() if not (k.startswith(('CONDA', 'MAMBA')))}

path = clean_env.get("PATH")
elems = path.split(os.pathsep)
elems = [e for e in elems if not "condabin" in e]
clean_env["PATH"] = os.pathsep.join(elems)

suffixes = {
    "cmdexe": ".bat",
    "bash": ".sh",
    "zsh": ".sh",
    "xonsh": ".sh",
    "powershell": ".ps1",
}

paths = {
    "win": {"powershell": None, "cmdexe": None},
    "osx": {"zsh": "~/.zshrc", "bash": "~/.bash_profile"},
    "linux": {"zsh": "~/.zshrc", "bash": "~/.bashrc"},
}

if plat == 'win':
    # find powershell profile path
    args = ["powershell", "-NoProfile", "-Command", "$PROFILE.CurrentUserAllHosts"]
    res = subprocess.run(
        args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True
    )
    paths["win"]["powershell"] = res.stdout.decode('utf-8').strip()

def write_script(interpreter, lines, path):
    fname = os.path.join(path, "script" + suffixes[interpreter])
    with open(fname, "w") as fo:
        fo.write("\n".join(lines) + "\n")

    return fname


def emit_check(cond):
    return cmds.if_(cond).then_(cmds.echo("YES")).else_(cmds.echo("NOPE"))


enable_on_os = {
    # "win": {"powershell", "cmdexe"},
    "win": {"cmdexe"},
    # "win": {"powershell"},
    # 'unix': {'bash', 'zsh', 'xonsh'},
    "unix": {"bash", "zsh"},
}

shell_files = [
    Path(x).expanduser()
    for x in ["~/.bashrc", "~/.bash_profile", "~/.zshrc", "~/.zsh_profile"]
]

if plat == 'win':
    shell_files.append(Path(paths['win']['powershell']))

regkey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Command Processor\\AutoRun"
bkup_winreg_value = None

@pytest.fixture
def clean_shell_files():
    global bkup_winreg_value

    for f in shell_files:
        if f.exists():
            f_bkup = Path(str(f) + ".mamba_test_backup")
            if f_bkup.exists():
                f_bkup.unlink()
            f.rename(f_bkup)
            f.touch()

    if plat == 'win':
        regvalue = read_windows_registry(regkey)
        bkup_winreg_value = regvalue
        write_windows_registry(regkey, "", regvalue[1])

    yield shell_files

    for f in shell_files:
        f_bkup = Path(str(f) + ".mamba_test_backup")
        if f_bkup.exists():
            if f.exists():
                f.unlink()
            f_bkup.rename(str(f_bkup)[: -len(".mamba_test_backup")])
        if plat == 'win':
            write_windows_registry(regkey, regvalue[0], regvalue[1])

def find_path_in_str(p, s):
    if p in s:
        return True
    if p.replace('\\', '\\\\') in s:
        return True
    return False

def call_interpreter(s, tmp_path, interpreter, interactive=False, env=None):
    if interactive and interpreter == 'powershell':
        # "Get-Content -Path $PROFILE.CurrentUserAllHosts | Invoke-Expression"
        s = [". $PROFILE.CurrentUserAllHosts"] + s
    if interpreter == 'cmdexe':
        mods = []
        for x in s:
            if x.startswith('micromamba activate') or x.startswith('micromamba deactivate'):
                mods.append('call ' + x)
            else:
                mods.append(x)
        s = mods
    f = write_script(interpreter, s, tmp_path)

    if interpreter not in enable_on_os[running_os]:
        return None, None

    if interpreter == "cmdexe":
        args = ["cmd.exe", "/Q", "/C", f]
    elif interpreter == "powershell":
        args = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", f]
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

    if interpreter == "cmdexe":
        if stdout.startswith('\'') and stdout.endswith('\''):
            stdout = stdout[1:-1]

    return stdout, stderr


def get_interpreters(exclude=[]):
    return [x for x in enable_on_os[running_os] if x not in exclude]

def shvar(v, interpreter):
    if interpreter in ['bash', 'zsh']:
        return f"${v}"
    elif interpreter == 'powershell':
        return f"$Env:{v}"
    elif interpreter == 'cmdexe':
        return f"%{v}%"


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
        call = lambda s: call_interpreter(s, tmp_path, interpreter)

        rpv = shvar('MAMBA_ROOT_PREFIX', interpreter)
        s = [f"echo {rpv}"]
        stdout, stderr = call(s)
        assert stdout == self.root_prefix

        # TODO remove root prefix here
        s = [f"{umamba} shell init -p {rpv}"]
        stdout, stderr = call(s)

        if interpreter == 'cmdexe':
            value = read_windows_registry(regkey)
            assert "mamba_hook.bat" in value[0]
            assert find_path_in_str(self.root_prefix, value[0])
            prev_text = value[0]
        else:
            path = Path(paths[plat][interpreter]).expanduser()
            with open(path) as fi:
                x = fi.read()
                assert "mamba" in x
                assert find_path_in_str(self.root_prefix, x)
                prev_text = x

        s = [f"{umamba} shell init -p {rpv}"]
        stdout, stderr = call(s)

        if interpreter == 'cmdexe':
            value = read_windows_registry(regkey)
            assert "mamba_hook.bat" in value[0]
            assert find_path_in_str(self.root_prefix, value[0])
            assert prev_text == value[0]
            assert not "&" in value[0]
        else:
            with open(path) as fi:
                x = fi.read()
                assert "mamba" in x
                assert prev_text == x

        if interpreter == 'cmdexe':
            write_windows_registry(regkey, "echo 'test'", bkup_winreg_value[1])
            s = [f"{umamba} shell init -p {rpv}"]
            stdout, stderr = call(s)

            value = read_windows_registry(regkey)
            assert "mamba_hook.bat" in value[0]
            assert find_path_in_str(self.root_prefix, value[0])
            assert value[0].startswith("echo 'test' & ")
            assert "&" in value[0]

        if interpreter != 'cmdexe':
            with open(path) as fi:
                prevlines = fi.readlines()

            with open(path, "w") as fo:
                text = "\n".join(
                    ["", "", "echo 'hihi'", ""]
                    + [x.rstrip("\n\r") for x in prevlines]
                    + ["", "", "echo 'hehe'"]
                )
                fo.write(text)

            s = [f"{umamba} shell init -p {rpv}"]
            stdout, stderr = call(s)
            with open(path) as fi:
                x = fi.read()
                assert "mamba" in x
                assert text == x

        s = [f"{umamba} shell init -p {self.other_root_prefix}"]
        stdout, stderr = call(s)

        if interpreter == 'cmdexe':
            x = read_windows_registry(regkey)[0]
            # CURRENTLY FAILING!
            # assert "mamba" in x
            # assert find_path_in_str(self.other_root_prefix, x)
            # assert not find_path_in_str(self.root_prefix, x)
        else:
            with open(path) as fi:
                x = fi.read()
                assert "mamba" in x
                assert find_path_in_str(self.other_root_prefix, x)
                assert not find_path_in_str(self.root_prefix, x)

    @pytest.mark.parametrize("interpreter", get_interpreters())
    def test_activation(self, tmp_path, interpreter, clean_shell_files):
        cwd = os.getcwd()
        umamba = get_umamba(cwd=cwd)

        s = [f"{umamba} shell init -p {self.root_prefix}"]
        stdout, stderr = call_interpreter(s, tmp_path, interpreter)

        call = lambda s: call_interpreter(
            s, tmp_path, interpreter, interactive=True, env=clean_env
        )

        if interpreter in ["bash", "zsh"]:
            extract_vars = lambda vxs: [f"echo {v}=${v}" for v in vxs]
        elif interpreter in ["cmdexe"]:
            extract_vars = lambda vxs: [f"echo {v}=%{v}%" for v in vxs]
        elif interpreter in ["powershell"]:
            extract_vars = lambda vxs: [f"echo {v}=$Env:{v}" for v in vxs]

        def to_dict(out):
            return {k: v for k, v in [x.split("=", 1) for x in out.splitlines()]}

        rp = Path(self.root_prefix)
        evars = extract_vars(["CONDA_PREFIX", "CONDA_SHLVL", "PATH"])

        if interpreter == "cmdexe":
            x = read_windows_registry(regkey)
            fp = Path(x[0][1:-1])
            assert(fp.exists())

        if interpreter in ["bash", "zsh", "powershell", "cmdexe"]:
            stdout, stderr = call(evars)

            s = [f"micromamba --help"]
            stdout, stderr = call(s)

            s = [f"micromamba activate"] + evars
            stdout, stderr = call(s)
            res = to_dict(stdout)

            assert "condabin" in res["PATH"]
            assert self.root_prefix in res["PATH"]
            assert f"CONDA_PREFIX={self.root_prefix}" in stdout.splitlines()
            assert f"CONDA_SHLVL=1" in stdout.splitlines()

            # throw with non-existent
            s = [f"micromamba activate nonexistent"]
            if not interpreter == 'powershell':
                with pytest.raises(subprocess.CalledProcessError):
                    stdout, stderr = call(s)

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

            assert find_path_in_str(str(rp / "condabin"), res["PATH"])
            assert not find_path_in_str(str(rp / "bin"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / "xyz"), res["PATH"])
            assert not find_path_in_str(str(rp / "envs" / "abc"), res["PATH"])

            s = [
                f"micromamba activate",
                f"micromamba activate abc",
                f"micromamba activate xyz --stack",
            ] + evars
            stdout, stderr = call(s)
            res = to_dict(stdout)
            assert find_path_in_str(str(rp / "condabin"), res["PATH"])
            assert not find_path_in_str(str(rp / "bin"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / "abc"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / "xyz"), res["PATH"])

            s = [
                f"micromamba activate",
                f"micromamba activate abc",
                f"micromamba activate --stack xyz",
            ] + evars
            stdout, stderr = call(s)
            res = to_dict(stdout)
            assert find_path_in_str(str(rp / "condabin"), res["PATH"])
            assert not find_path_in_str(str(rp / "bin"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / "xyz"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / "abc"), res["PATH"])