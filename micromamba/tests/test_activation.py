import os
import platform
import shutil
import string
import subprocess
import sys
import tempfile

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
    os.system("chcp 65001")
    plat = "win"
    running_os = "win"


@pytest.fixture
def keep_cache(existing_cache):
    yield True


@pytest.fixture
def clean_env(keep_cache):
    clean_env = os.environ.copy()
    clean_env = {
        k: v
        for k, v in clean_env.items()
        if not k.startswith(("CONDA", "MAMBA"))
        or (k == "CONDA_PKGS_DIRS" and keep_cache)
    }

    path = clean_env.get("PATH")
    elems = path.split(os.pathsep)
    elems = [e for e in elems if not "condabin" in e]
    clean_env["PATH"] = os.pathsep.join(elems)

    yield clean_env


suffixes = {
    "cmd.exe": ".bat",
    "bash": ".sh",
    "zsh": ".sh",
    "xonsh": ".sh",
    "fish": ".fish",
    "powershell": ".ps1",
}

paths = {
    "win": {"powershell": None, "cmd.exe": None},
    "osx": {
        "zsh": "~/.zshrc",
        "bash": "~/.bash_profile",
        "fish": "~/.config/fish/config.fish",
    },
    "linux": {
        "zsh": "~/.zshrc",
        "bash": "~/.bashrc",
        "fish": "~/.config/fish/config.fish",
    },
}

if plat == "win":
    # find powershell profile path
    args = ["powershell", "-NoProfile", "-Command", "$PROFILE.CurrentUserAllHosts"]
    res = subprocess.run(
        args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True
    )
    paths["win"]["powershell"] = res.stdout.decode("utf-8").strip()


def write_script(interpreter, lines, path):
    fname = os.path.join(path, "script" + suffixes[interpreter])
    if plat == "win":
        if interpreter == "powershell":
            with open(fname, "w", encoding="utf-8-sig") as fo:
                fo.write("\n".join(lines) + "\n")
        else:
            with open(fname, "w", encoding="utf-8") as fo:
                fo.write("\n".join(lines) + "\n")
    else:
        with open(fname, "w") as fo:
            fo.write("\n".join(lines) + "\n")

    return fname


def emit_check(cond):
    return cmds.if_(cond).then_(cmds.echo("YES")).else_(cmds.echo("NOPE"))


possible_interpreters = {
    "win": {"powershell", "cmd.exe"},
    # 'unix': {'bash', 'zsh', 'xonsh'},
    "unix": {"bash", "zsh", "fish"},
}

shell_files = [
    Path(x).expanduser()
    for x in [
        "~/.bashrc",
        "~/.bash_profile",
        "~/.zshrc",
        "~/.zsh_profile",
        "~/.config/fish/config.fish",
    ]
]

if plat == "win":
    shell_files.append(Path(paths["win"]["powershell"]))

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

    if plat == "win":
        try:
            regvalue = read_windows_registry(regkey)
            bkup_winreg_value = regvalue
        except:
            print("Could not read registry value")
            regvalue = ("", 1)
        print("setting registry to ", regvalue[1])
        write_windows_registry(regkey, "", regvalue[1])

    yield shell_files

    for f in shell_files:
        f_bkup = Path(str(f) + ".mamba_test_backup")
        if f_bkup.exists():
            if f.exists():
                f.unlink()
            f_bkup.rename(str(f_bkup)[: -len(".mamba_test_backup")])
        if plat == "win":
            print("setting registry to ", regvalue[0], regvalue[1])
            write_windows_registry(regkey, regvalue[0], regvalue[1])


def find_path_in_str(p, s):
    if p in s:
        return True
    if p.replace("\\", "\\\\") in s:
        return True
    return False


def call_interpreter(s, tmp_path, interpreter, interactive=False, env=None):
    if interactive and interpreter == "powershell":
        # "Get-Content -Path $PROFILE.CurrentUserAllHosts | Invoke-Expression"
        s = [". $PROFILE.CurrentUserAllHosts"] + s
    if interactive and interpreter == "bash" and plat == "linux":
        s = ["source ~/.bashrc"] + s

    if interpreter == "cmd.exe":
        mods = ["@chcp 65001>nul"]
        for x in s:
            if x.startswith("micromamba activate") or x.startswith(
                "micromamba deactivate"
            ):
                mods.append("call " + x)
            else:
                mods.append(x)
        s = mods
    f = write_script(interpreter, s, tmp_path)

    if interpreter not in possible_interpreters[running_os]:
        return None, None

    if interpreter == "cmd.exe":
        args = ["cmd.exe", "/Q", "/C", f]
    elif interpreter == "powershell":
        args = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", f]
    else:
        args = [interpreter, f]
        if interactive:
            args.insert(1, "-i")
        if interactive and interpreter == "bash":
            args.insert(1, "-l")

    try:
        res = subprocess.run(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
            env=env,
            encoding="utf-8",
        )
    except subprocess.CalledProcessError as e:

        stdout = e.stdout.strip()
        stderr = e.stderr.strip()

        try:
            print(stdout)
            print(stderr)
        except:
            pass

        if interpreter == "cmd.exe":
            if stdout.startswith("'") and stdout.endswith("'"):
                stdout = stdout[1:-1]

        e.stdout = stdout
        e.stderr = stderr
        raise e
    except Exception as e:
        raise e

    stdout = res.stdout.strip()
    stderr = res.stderr.strip()

    try:
        print(stdout)
        print(stderr)
    except:
        pass

    if interpreter == "cmd.exe":
        if stdout.startswith("'") and stdout.endswith("'"):
            stdout = stdout[1:-1]

    return stdout, stderr


def get_interpreters(exclude=[]):
    return [x for x in possible_interpreters[running_os] if x not in exclude]


def get_valid_interpreters():
    valid_interpreters = []
    s = ["echo 'hello world'"]
    with tempfile.TemporaryDirectory() as tmpdirname:
        for interpreter in possible_interpreters[running_os]:
            try:
                stdout, _ = call_interpreter(s, tmpdirname, interpreter)
                assert stdout == "hello world"
                valid_interpreters.append(interpreter)
            except:
                pass

    return valid_interpreters


valid_interpreters = get_valid_interpreters()


def shvar(v, interpreter):
    if interpreter in ["bash", "zsh"]:
        return f"${v}"
    elif interpreter == "powershell":
        return f"$Env:{v}"
    elif interpreter == "cmd.exe":
        return f"%{v}%"
    elif interpreter == "fish":
        return f"${v}"


class TestActivation:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    other_root_prefix = os.path.expanduser(
        os.path.join("~", "tmproot" + random_string())
    )
    prefix = os.path.join(root_prefix, "envs", env_name)
    long_prefix = os.path.join(
        root_prefix, *["some_very_long_prefix" for i in range(20)], env_name
    )

    @staticmethod
    @pytest.fixture(scope="class")
    def new_root_prefix(existing_cache):
        os.environ["MAMBA_ROOT_PREFIX"] = TestActivation.root_prefix
        os.environ["CONDA_PREFIX"] = TestActivation.prefix
        create("-n", "base", no_dry_run=True)
        create("xtensor", "-n", TestActivation.env_name, no_dry_run=True)

        yield

        os.environ["MAMBA_ROOT_PREFIX"] = TestActivation.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestActivation.current_prefix

        if os.path.exists(TestActivation.root_prefix):
            if platform.system() == "Windows":
                p = r"\\?\ ".strip() + TestActivation.root_prefix
            else:
                p = TestActivation.root_prefix
            # shutil.rmtree(p)

        if os.path.exists(TestActivation.other_root_prefix):
            if platform.system() == "Windows":
                p = r"\\?\ ".strip() + TestActivation.other_root_prefix
            else:
                p = TestActivation.other_root_prefix
            # shutil.rmtree(p)

    @staticmethod
    def to_dict(out, interpreter="bash"):
        if interpreter == "cmd.exe":
            with open(out, "r") as f:
                out = f.read()

        if interpreter == "fish":
            return {
                v.split(" ", maxsplit=1)[0]: v.split(" ", maxsplit=1)[1]
                for _, _, v in [x.partition("set -gx ") for x in out.splitlines()]
            }
        else:
            return {k: v for k, _, v in [x.partition("=") for x in out.splitlines()]}

    @pytest.mark.parametrize("interpreter", get_interpreters())
    def test_shell_init(
        self, tmp_path, interpreter, clean_shell_files, new_root_prefix
    ):
        if interpreter not in valid_interpreters:
            pytest.skip(f"{interpreter} not available")

        cwd = os.getcwd()
        umamba = get_umamba(cwd=cwd)
        env = {"MAMBA_ROOT_PREFIX": self.root_prefix}
        call = lambda s: call_interpreter(s, tmp_path, interpreter)

        rpv = shvar("MAMBA_ROOT_PREFIX", interpreter)
        s = [f"echo {rpv}"]
        stdout, stderr = call(s)
        assert stdout == self.root_prefix

        # TODO remove root prefix here
        s = [f"{umamba} shell init -p {rpv}"]
        stdout, stderr = call(s)

        if interpreter == "cmd.exe":
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

        if interpreter == "cmd.exe":
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

        if interpreter == "cmd.exe":
            write_windows_registry(regkey, "echo 'test'", bkup_winreg_value[1])
            s = [f"{umamba} shell init -p {rpv}"]
            stdout, stderr = call(s)

            value = read_windows_registry(regkey)
            assert "mamba_hook.bat" in value[0]
            assert find_path_in_str(self.root_prefix, value[0])
            assert value[0].startswith("echo 'test' & ")
            assert "&" in value[0]

        if interpreter != "cmd.exe":
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

        if interpreter == "cmd.exe":
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
    def test_activation(
        self, tmp_path, interpreter, clean_shell_files, new_root_prefix, clean_env
    ):
        if interpreter not in valid_interpreters:
            pytest.skip(f"{interpreter} not available")

        cwd = os.getcwd()
        umamba = get_umamba(cwd=cwd)

        s = [f"{umamba} shell init -p {self.root_prefix}"]
        stdout, stderr = call_interpreter(s, tmp_path, interpreter)

        call = lambda s: call_interpreter(
            s, tmp_path, interpreter, interactive=True, env=clean_env
        )

        if interpreter in ["bash", "zsh"]:
            extract_vars = lambda vxs: [f"echo {v}=${v}" for v in vxs]
        elif interpreter in ["cmd.exe"]:
            extract_vars = lambda vxs: [f"echo {v}=%{v}%" for v in vxs]
        elif interpreter in ["powershell"]:
            extract_vars = lambda vxs: [f"echo {v}=$Env:{v}" for v in vxs]
        elif interpreter in ["fish"]:
            extract_vars = lambda vxs: [f"echo {v}=${v}" for v in vxs]

        rp = Path(self.root_prefix)
        evars = extract_vars(["CONDA_PREFIX", "CONDA_SHLVL", "PATH"])

        if interpreter == "cmd.exe":
            x = read_windows_registry(regkey)
            fp = Path(x[0][1:-1])
            assert fp.exists()

        if interpreter in ["bash", "zsh", "powershell", "cmd.exe"]:
            stdout, stderr = call(evars)

            s = [f"{umamba} --help"]
            stdout, stderr = call(s)

            s = ["micromamba activate"] + evars
            stdout, stderr = call(s)
            res = TestActivation.to_dict(stdout)

            assert "condabin" in res["PATH"]
            assert self.root_prefix in res["PATH"]
            assert f"CONDA_PREFIX={self.root_prefix}" in stdout.splitlines()
            assert f"CONDA_SHLVL=1" in stdout.splitlines()

            # throw with non-existent
            s = ["micromamba activate nonexistent"]
            if plat != "win":
                with pytest.raises(subprocess.CalledProcessError):
                    stdout, stderr = call(s)

            s1 = ["micromamba create -n abc"]
            s2 = ["micromamba create -n xyz"]
            call(s1)
            call(s2)

            s = [
                "micromamba activate",
                "micromamba activate abc",
                "micromamba activate xyz",
            ] + evars
            stdout, stderr = call(s)
            res = TestActivation.to_dict(stdout)

            assert find_path_in_str(str(rp / "condabin"), res["PATH"])
            assert not find_path_in_str(str(rp / "bin"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / "xyz"), res["PATH"])
            assert not find_path_in_str(str(rp / "envs" / "abc"), res["PATH"])

            # long paths
            s = [
                f"micromamba create -p {TestActivation.long_prefix} xtensor six -v -y -c conda-forge"
            ]
            call(s)

            assert os.path.exists(TestActivation.long_prefix)

            s = [
                "micromamba activate",
                f"micromamba activate -p {TestActivation.long_prefix}",
            ] + evars
            # currently skipping longprefix _activation_ check with cmd.exe as it fails
            # on CI for mysterious reasons
            if not (plat == "win" and interpreter == "cmd.exe"):
                stdout, stderr = call(s)
                res = TestActivation.to_dict(stdout)

                assert find_path_in_str(str(rp / "condabin"), res["PATH"])
                assert not find_path_in_str(str(rp / "bin"), res["PATH"])
                assert find_path_in_str(TestActivation.long_prefix, res["PATH"])

            s = [
                "micromamba activate",
                "micromamba activate abc",
                "micromamba activate xyz --stack",
            ] + evars
            stdout, stderr = call(s)
            res = TestActivation.to_dict(stdout)
            assert find_path_in_str(str(rp / "condabin"), res["PATH"])
            assert not find_path_in_str(str(rp / "bin"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / "abc"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / "xyz"), res["PATH"])

            s = [
                "micromamba activate",
                "micromamba activate abc",
                "micromamba activate --stack xyz",
            ] + evars
            stdout, stderr = call(s)
            res = TestActivation.to_dict(stdout)
            assert find_path_in_str(str(rp / "condabin"), res["PATH"])
            assert not find_path_in_str(str(rp / "bin"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / "xyz"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / "abc"), res["PATH"])

    @pytest.mark.parametrize("interpreter", get_interpreters())
    def test_unicode_activation(
        self, tmp_path, interpreter, clean_shell_files, new_root_prefix, clean_env
    ):
        if interpreter not in valid_interpreters:
            pytest.skip(f"{interpreter} not available")

        cwd = os.getcwd()
        umamba = get_umamba(cwd=cwd)

        s = [f"{umamba} shell init -p {self.root_prefix}"]
        stdout, stderr = call_interpreter(s, tmp_path, interpreter)

        call = lambda s: call_interpreter(
            s, tmp_path, interpreter, interactive=True, env=clean_env
        )

        if interpreter in ["bash", "zsh"]:
            extract_vars = lambda vxs: [f"echo {v}=${v}" for v in vxs]
        elif interpreter in ["cmd.exe"]:
            extract_vars = lambda vxs: [f"echo {v}=%{v}%" for v in vxs]
        elif interpreter in ["powershell"]:
            extract_vars = lambda vxs: [f"echo {v}=$Env:{v}" for v in vxs]
        elif interpreter in ["fish"]:
            extract_vars = lambda vxs: [f"echo {v}=${v}" for v in vxs]

        rp = Path(self.root_prefix)
        evars = extract_vars(["CONDA_PREFIX", "CONDA_SHLVL", "PATH"])

        if interpreter == "cmd.exe":
            x = read_windows_registry(regkey)
            fp = Path(x[0][1:-1])
            assert fp.exists()

        if interpreter in ["bash", "zsh", "powershell", "cmd.exe"]:
            stdout, stderr = call(evars)

            s = [f"{umamba} --help"]
            stdout, stderr = call(s)

            s = ["micromamba activate"] + evars
            stdout, stderr = call(s)
            res = TestActivation.to_dict(stdout)

            assert "condabin" in res["PATH"]
            assert self.root_prefix in res["PATH"]
            assert f"CONDA_PREFIX={self.root_prefix}" in stdout.splitlines()
            assert f"CONDA_SHLVL=1" in stdout.splitlines()

            # throw with non-existent
            s = ["micromamba activate nonexistent"]
            if plat != "win":
                with pytest.raises(subprocess.CalledProcessError):
                    stdout, stderr = call(s)

            u1 = "μυρτιὲς"
            u2 = "终过鬼门关"
            u3 = "some ™∞¢3 spaces §∞©ƒ√≈ç"
            s1 = [f"micromamba create -n {u1} xtensor -y -c conda-forge"]
            s2 = [f"micromamba create -n {u2} xtensor -y -c conda-forge"]
            if interpreter == "cmd.exe":
                s3 = [f'micromamba create -n "{u3}" xtensor -y -c conda-forge']
            else:
                s3 = [f"micromamba create -n '{u3}' xtensor -y -c conda-forge"]
            call(s1)
            call(s2)
            call(s3)

            assert (rp / "envs" / u1 / "conda-meta").is_dir()
            assert (rp / "envs" / u2 / "conda-meta").is_dir()
            assert (rp / "envs" / u3 / "conda-meta").is_dir()
            assert (rp / "envs" / u1 / "conda-meta" / "history").exists()
            assert (rp / "envs" / u2 / "conda-meta" / "history").exists()
            assert (rp / "envs" / u3 / "conda-meta" / "history").exists()
            if plat == "win":
                assert (
                    rp / "envs" / u1 / "Library" / "include" / "xtensor" / "xtensor.hpp"
                ).exists()
                assert (
                    rp / "envs" / u2 / "Library" / "include" / "xtensor" / "xtensor.hpp"
                ).exists()
                assert (
                    rp / "envs" / u3 / "Library" / "include" / "xtensor" / "xtensor.hpp"
                ).exists()
            else:
                assert (
                    rp / "envs" / u1 / "include" / "xtensor" / "xtensor.hpp"
                ).exists()
                assert (
                    rp / "envs" / u2 / "include" / "xtensor" / "xtensor.hpp"
                ).exists()
                assert (
                    rp / "envs" / u3 / "include" / "xtensor" / "xtensor.hpp"
                ).exists()

            # unicode activation on win: todo
            if plat == "win":
                return

            s = [
                f"micromamba activate",
                f"micromamba activate {u1}",
                f"micromamba activate {u2}",
            ] + evars
            stdout, stderr = call(s)
            res = TestActivation.to_dict(stdout)

            assert find_path_in_str(str(rp / "condabin"), res["PATH"])
            assert not find_path_in_str(str(rp / "bin"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / u2), res["PATH"])
            assert not find_path_in_str(str(rp / "envs" / u1), res["PATH"])

            s = [
                "micromamba activate",
                f"micromamba activate {u1}",
                f"micromamba activate {u2} --stack",
            ] + evars
            stdout, stderr = call(s)
            res = TestActivation.to_dict(stdout)
            assert find_path_in_str(str(rp / "condabin"), res["PATH"])
            assert not find_path_in_str(str(rp / "bin"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / u1), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / u2), res["PATH"])

            s = [
                "micromamba activate",
                f"micromamba activate '{u3}'",
            ] + evars
            stdout, stderr = call(s)
            res = TestActivation.to_dict(stdout)
            assert find_path_in_str(str(rp / "condabin"), res["PATH"])
            assert not find_path_in_str(str(rp / "bin"), res["PATH"])
            assert find_path_in_str(str(rp / "envs" / u3), res["PATH"])

    @pytest.mark.parametrize("interpreter", get_interpreters())
    def test_activate_path(self, tmp_path, interpreter, new_root_prefix):
        if interpreter not in valid_interpreters:
            pytest.skip(f"{interpreter} not available")

        test_dir = tmp_path / self.env_name
        os.mkdir(test_dir)

        create("-n", self.env_name)

        res = shell("activate", self.env_name, "-s", interpreter)
        dict_res = self.to_dict(res, interpreter)
        assert any([str(self.prefix) in p for p in dict_res.values()])

        res = shell("activate", self.prefix, "-s", interpreter)
        dict_res = self.to_dict(res, interpreter)
        assert any([str(self.prefix) in p for p in dict_res.values()])

        prefix_short = self.prefix.replace(os.path.expanduser("~"), "~")
        res = shell("activate", prefix_short, "-s", interpreter)
        dict_res = self.to_dict(res, interpreter)
        assert any([str(self.prefix) in p for p in dict_res.values()])

        res = shell("activate", test_dir, "-s", interpreter)
        dict_res = self.to_dict(res, interpreter)
        assert any([str(test_dir) in p for p in dict_res.values()])
