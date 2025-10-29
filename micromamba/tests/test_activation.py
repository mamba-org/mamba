import os
import pathlib
import platform
import shutil
import subprocess
import tempfile
from pathlib import Path, PurePosixPath, PureWindowsPath

import pytest

from . import helpers

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


suffixes = {
    "cmd.exe": ".bat",
    "bash": ".sh",
    "zsh": ".sh",
    "tcsh": ".csh",
    "xonsh": ".sh",
    "fish": ".fish",
    "powershell": ".ps1",
    "nu": ".nu",
}

powershell_cmd = None

if plat == "win":

    def detect_powershell():
        try:
            result = subprocess.run(
                ["pwsh", "-Command", "$PSVersionTable.PSVersion.Major"],
                capture_output=True,
                text=True,
                check=True,
            )
            version = int(result.stdout.strip())
            if version >= 7:
                return "pwsh"
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass

        try:
            result = subprocess.run(
                ["powershell", "-Command", "$PSVersionTable.PSVersion.Major"],
                capture_output=True,
                text=True,
                check=True,
            )
            version = int(result.stdout.strip())
            if version >= 5:
                return "powershell"
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass

        raise OSError("No compatible PowerShell installation found.")

    powershell_cmd = detect_powershell()


class WindowsProfiles:
    def __getitem__(self, shell: str) -> str:
        if shell == "powershell":
            # find powershell profile path dynamically
            args = [
                powershell_cmd,
                "-NoProfile",
                "-Command",
                "$PROFILE.CurrentUserAllHosts",
            ]
            res = subprocess.run(args, capture_output=True, check=True)
            return res.stdout.decode("utf-8").strip()
        elif shell == "cmd.exe":
            return None
        raise KeyError(f"Invalid shell: {shell}")


paths = {
    "win": WindowsProfiles(),
    "osx": {
        "zsh": "~/.zshrc",
        "bash": "~/.bash_profile",
        "xonsh": "~/.xonshrc",
        "tcsh": "~/.tcshrc",
        "fish": "~/.config/fish/config.fish",
        "nu": "~/.config/nushell/config.nu",
    },
    "linux": {
        "zsh": "~/.zshrc",
        "bash": "~/.bashrc",
        "xonsh": "~/.xonshrc",
        "tcsh": "~/.tcshrc",
        "fish": "~/.config/fish/config.fish",
        "nu": "~/.config/nushell/config.nu",
    },
}


def xonsh_shell_args(interpreter):
    # In macOS, the parent process name is "Python" and not "xonsh" like in Linux.
    # Thus, we need to specify the shell explicitly.
    return "-s xonsh" if interpreter == "xonsh" and plat == "osx" else ""


def extract_vars(vxs, interpreter):
    return [f"echo {v}={shvar(v, interpreter)}" for v in vxs]


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


possible_interpreters = {
    "win": {"powershell", "cmd.exe", "bash", "nu"},
    "unix": {"bash", "zsh", "fish", "xonsh", "tcsh", "nu"},
}


regkey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Command Processor\\AutoRun"


@pytest.fixture
def winreg_value():
    if plat == "win":
        try:
            saved_winreg_value = helpers.read_windows_registry(regkey)
        except Exception:
            print("Could not read registry value")
            saved_winreg_value = ("", 1)

        new_winreg_value = ("", saved_winreg_value[1])
        print("setting registry to ", new_winreg_value)
        helpers.write_windows_registry(regkey, *new_winreg_value)

        yield new_winreg_value

        print("setting registry to ", saved_winreg_value)
        helpers.write_windows_registry(regkey, *saved_winreg_value)
    else:
        yield None


def find_path_in_str(p, s):
    if isinstance(p, Path):
        p = str(p)
    if p in s:
        return True
    if p.replace("\\", "\\\\") in s:
        return True
    return False


def format_path(p, interpreter):
    if plat == "win" and interpreter == "bash":
        return str(PurePosixPath(PureWindowsPath(p)))
    else:
        return str(p)


def call_interpreter(s, tmp_path, interpreter, interactive=False, env=None):
    if interactive and interpreter == "powershell":
        # "Get-Content -Path $PROFILE.CurrentUserAllHosts | Invoke-Expression"
        s = [". $PROFILE.CurrentUserAllHosts"] + s
    if interactive and interpreter == "bash" and plat == "linux":
        s = ["source ~/.bashrc"] + s

    if interpreter == "cmd.exe":
        mods = ["@chcp 65001>nul"]
        umamba = helpers.get_umamba()
        mamba_name = Path(umamba).stem
        for x in s:
            if x.startswith(f"{mamba_name} activate") or x.startswith(f"{mamba_name} deactivate"):
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
        args = [powershell_cmd, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", f]
    elif interpreter == "bash" and plat == "win":
        args = [os.path.join(os.environ["PROGRAMFILES"], "Git", "bin", "bash.exe"), f]
    else:
        args = [interpreter, f]
        if interactive:
            args.insert(1, "-i")
        if interactive and interpreter == "bash":
            args.insert(1, "-l")

    try:
        res = subprocess.run(
            args,
            capture_output=True,
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
        except Exception:
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
    except Exception:
        pass

    if interpreter == "cmd.exe":
        if stdout.startswith("'") and stdout.endswith("'"):
            stdout = stdout[1:-1]

    return stdout, stderr


def get_interpreters(exclude=None):
    if exclude is None:
        exclude = []
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
            except Exception:
                pass

    return valid_interpreters


valid_interpreters = get_valid_interpreters()


def get_self_update_interpreters():
    if plat == "win":
        return ["cmd.exe", "powershell", "bash"]
    if plat == "osx":
        return ["zsh", "bash"]
    else:
        return ["bash"]


def shvar(v, interpreter):
    if interpreter in ["bash", "zsh", "xonsh", "fish", "tcsh", "dash"]:
        return f"${v}"
    elif interpreter == "powershell":
        return f"$Env:{v}"
    elif interpreter == "cmd.exe":
        return f"%{v}%"
    elif interpreter == "nu":
        return f"$env.{v}"


def env_to_dict(out, interpreter="bash"):
    if interpreter == "cmd.exe":
        with open(out) as f:
            out = f.read()

    if interpreter == "fish":
        return {
            v.split(" ", maxsplit=1)[0]: v.split(" ", maxsplit=1)[1]
            for _, _, v in [x.partition("set -gx ") for x in out.splitlines()]
        }
    elif interpreter in ["csh", "tcsh"]:
        res = {}
        for line in out.splitlines():
            line = line.removesuffix(";")
            if line.startswith("set "):
                k, v = line.split(" ")[1].split("=")
            elif line.startswith("setenv "):
                _, k, v = line.strip().split(maxsplit=2)
            res[k] = v
        return res
    else:
        return {k: v for k, _, v in [x.partition("=") for x in out.splitlines()]}


@pytest.mark.parametrize("interpreter", get_interpreters())
def test_shell_init(
    tmp_home,
    winreg_value,
    tmp_root_prefix,
    tmp_path,
    interpreter,
):
    # TODO enable these tests also on win + bash!
    if interpreter not in valid_interpreters or (plat == "win" and interpreter == "bash"):
        pytest.skip(f"{interpreter} not available")

    umamba = helpers.get_umamba()
    run_dir = tmp_path / "rundir"
    run_dir.mkdir()

    def call(s):
        return call_interpreter(s, run_dir, interpreter)

    rpv = shvar("MAMBA_ROOT_PREFIX", interpreter)
    s = [f"echo {rpv}"]
    stdout, stderr = call(s)
    assert stdout == str(tmp_root_prefix)

    s = [f"{umamba} shell init -r {rpv} {xonsh_shell_args(interpreter)}"]
    stdout, stderr = call(s)

    if interpreter == "cmd.exe":
        value = helpers.read_windows_registry(regkey)
        assert "mamba_hook.bat" in value[0]
        assert find_path_in_str(tmp_root_prefix, value[0])
        prev_text = value[0]
    else:
        path = Path(paths[plat][interpreter]).expanduser()
        with open(path) as fi:
            x = fi.read()
            assert "mamba" in x
            assert find_path_in_str(tmp_root_prefix, x)
            prev_text = x

    s = [f"{umamba} shell init -r {rpv} {xonsh_shell_args(interpreter)}"]
    stdout, stderr = call(s)

    if interpreter == "cmd.exe":
        value = helpers.read_windows_registry(regkey)
        assert "mamba_hook.bat" in value[0]
        assert find_path_in_str(tmp_root_prefix, value[0])
        assert prev_text == value[0]
        assert "&" not in value[0]
    else:
        with open(path) as fi:
            x = fi.read()
            assert "mamba" in x
            assert prev_text == x

    if interpreter == "cmd.exe":
        helpers.write_windows_registry(regkey, "echo 'test'", winreg_value[1])
        s = [f"{umamba} shell init -r {rpv}"]
        stdout, stderr = call(s)

        value = helpers.read_windows_registry(regkey)
        assert "mamba_hook.bat" in value[0]
        assert find_path_in_str(tmp_root_prefix, value[0])
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

        s = [f"{umamba} shell init -r {rpv}"]
        stdout, stderr = call(s)
        with open(path) as fi:
            x = fi.read()
            assert "mamba" in x
            assert text == x

    other_root_prefix = tmp_path / "prefix"
    other_root_prefix.mkdir()
    s = [f"{umamba} shell init -r {other_root_prefix} {xonsh_shell_args(interpreter)}"]
    stdout, stderr = call(s)

    if interpreter == "cmd.exe":
        x = helpers.read_windows_registry(regkey)[0]
        assert "mamba" in x
        assert find_path_in_str(other_root_prefix, x)
        assert not find_path_in_str(tmp_root_prefix, x)
    else:
        with open(path) as fi:
            x = fi.read()
            assert "mamba" in x
            assert find_path_in_str(other_root_prefix, x)
            assert not find_path_in_str(tmp_root_prefix, x)


@pytest.mark.parametrize("interpreter", get_interpreters())
def test_shell_init_deinit_root_prefix_files(
    tmp_home,
    tmp_root_prefix,
    tmp_path,
    interpreter,
):
    if interpreter not in valid_interpreters or (plat == "win" and interpreter == "bash"):
        pytest.skip(f"{interpreter} not available")

    umamba = helpers.get_umamba()

    if interpreter == "bash" or interpreter == "zsh":
        files = [tmp_root_prefix / "etc" / "profile.d" / "mamba.sh"]
    elif interpreter == "cmd.exe":
        files = [
            tmp_root_prefix / "condabin" / "mamba_hook.bat",
            tmp_root_prefix / "condabin" / "mamba.bat",
            tmp_root_prefix / "condabin" / "_mamba_activate.bat",
            tmp_root_prefix / "condabin" / "activate.bat",
        ]
    elif interpreter == "powershell":
        files = [
            tmp_root_prefix / "condabin" / "mamba_hook.ps1",
            tmp_root_prefix / "condabin" / "Mamba.psm1",
        ]
    elif interpreter == "fish":
        files = [tmp_root_prefix / "etc" / "fish" / "conf.d" / "mamba.fish"]
    elif interpreter == "xonsh":
        files = [tmp_root_prefix / "etc" / "profile.d" / "mamba.xsh"]
    elif interpreter in ["csh", "tcsh"]:
        files = [tmp_root_prefix / "etc" / "profile.d" / "mamba.csh"]
    elif interpreter == "nu":
        files = []  # moved to ~/.config/nushell.nu controlled by mamba activation
    else:
        raise ValueError(f"Unknown shell {interpreter}")

    def call(command):
        return call_interpreter(command, tmp_path, interpreter)

    s = [f"{umamba} shell init -r {tmp_root_prefix} {xonsh_shell_args(interpreter)}"]
    call(s)

    for file in files:
        assert file.exists()

    s = [f"{umamba} shell deinit -r {tmp_root_prefix} {xonsh_shell_args(interpreter)}"]
    call(s)

    for file in files:
        assert not file.exists()


def test_shell_init_deinit_contents_cmdexe(
    tmp_home,
    winreg_value,
    tmp_root_prefix,
    tmp_path,
):
    interpreter = "cmd.exe"
    if interpreter not in valid_interpreters:
        pytest.skip(f"{interpreter} not available")

    umamba = helpers.get_umamba()

    def call(command):
        return call_interpreter(command, tmp_path, interpreter)

    prev_value = helpers.read_windows_registry(regkey)
    assert "mamba_hook.bat" not in prev_value[0]
    assert not find_path_in_str(tmp_root_prefix, prev_value[0])

    s = [f"{umamba} shell init -r {tmp_root_prefix} {xonsh_shell_args(interpreter)}"]
    call(s)

    value_after_init = helpers.read_windows_registry(regkey)
    assert "mamba_hook.bat" in value_after_init[0]
    assert find_path_in_str(tmp_root_prefix, value_after_init[0])

    s = [f"{umamba} shell deinit -r {tmp_root_prefix} {xonsh_shell_args(interpreter)}"]
    call(s)

    value_after_deinit = helpers.read_windows_registry(regkey)
    assert value_after_deinit == prev_value


@pytest.mark.parametrize("interpreter", get_interpreters(exclude=["cmd.exe"]))
def test_shell_init_deinit_contents(
    tmp_home,
    tmp_root_prefix,
    tmp_path,
    interpreter,
):
    if interpreter not in valid_interpreters or (plat == "win" and interpreter == "bash"):
        pytest.skip(f"{interpreter} not available")

    umamba = helpers.get_umamba()

    def call(command):
        return call_interpreter(command, tmp_path, interpreter)

    path = Path(paths[plat][interpreter]).expanduser()

    if os.path.exists(path):
        with open(path) as fi:
            prev_rc_contents = fi.read()
    else:
        prev_rc_contents = ""
    if interpreter == "powershell":
        assert "#region mamba initialize" not in prev_rc_contents
    else:
        assert "# >>> mamba initialize >>>" not in prev_rc_contents
    assert not find_path_in_str(tmp_root_prefix, prev_rc_contents)

    s = [f"{umamba} shell init -r {tmp_root_prefix} {xonsh_shell_args(interpreter)}"]
    call(s)

    with open(path) as fi:
        rc_contents_after_init = fi.read()
        if interpreter == "powershell":
            assert "#region mamba initialize" in rc_contents_after_init
        else:
            assert "# >>> mamba initialize >>>" in rc_contents_after_init
        assert find_path_in_str(tmp_root_prefix, rc_contents_after_init)

    s = [f"{umamba} shell deinit -r {tmp_root_prefix} {xonsh_shell_args(interpreter)}"]
    call(s)

    if os.path.exists(path):
        with open(path) as fi:
            rc_contents_after_deinit = fi.read()
    else:
        rc_contents_after_deinit = ""
    assert rc_contents_after_deinit == prev_rc_contents


@pytest.mark.parametrize("interpreter", get_interpreters())
def test_env_activation(tmp_home, winreg_value, tmp_root_prefix, tmp_path, interpreter):
    if interpreter not in valid_interpreters or (plat == "win" and interpreter == "bash"):
        pytest.skip(f"{interpreter} not available")

    umamba = helpers.get_umamba()
    mamba_name = Path(umamba).stem

    s = [f"{umamba} shell init -r {tmp_root_prefix}"]
    stdout, stderr = call_interpreter(s, tmp_path, interpreter)

    def call(s):
        return call_interpreter(s, tmp_path, interpreter, interactive=True)

    evars = extract_vars(["CONDA_PREFIX", "CONDA_SHLVL", "PATH"], interpreter)

    if interpreter == "cmd.exe":
        x = helpers.read_windows_registry(regkey)
        fp = Path(x[0][1:-1])
        assert fp.exists()

    if interpreter in ["bash", "zsh", "powershell", "cmd.exe"]:
        stdout, stderr = call(evars)

        s = [f"{umamba} --help"]
        stdout, stderr = call(s)

        s = [f"{mamba_name} activate"] + evars
        stdout, stderr = call(s)
        res = env_to_dict(stdout)

        assert "condabin" in res["PATH"]
        assert str(tmp_root_prefix) in res["PATH"]
        assert f"CONDA_PREFIX={tmp_root_prefix}" in stdout.splitlines()
        assert "CONDA_SHLVL=1" in stdout.splitlines()

        # throw with non-existent
        if plat != "win":
            with pytest.raises(subprocess.CalledProcessError):
                stdout, stderr = call([f"{mamba_name} activate nonexistent"])

        call([f"{mamba_name} create -n abc -y"])
        call([f"{mamba_name} create -n xyz -y"])

        s = [
            f"{mamba_name} activate",
            f"{mamba_name} activate abc",
            f"{mamba_name} activate xyz",
        ] + evars
        stdout, stderr = call(s)
        res = env_to_dict(stdout)
        assert find_path_in_str(tmp_root_prefix / "condabin", res["PATH"])
        assert not find_path_in_str(tmp_root_prefix / "bin", res["PATH"])
        assert find_path_in_str(tmp_root_prefix / "envs" / "xyz", res["PATH"])
        assert not find_path_in_str(tmp_root_prefix / "envs" / "abc", res["PATH"])

        s = [
            f"{mamba_name} activate",
            f"{mamba_name} activate abc",
            f"{mamba_name} activate --stack xyz",
        ] + evars
        stdout, stderr = call(s)
        res = env_to_dict(stdout)
        assert find_path_in_str(tmp_root_prefix / "condabin", res["PATH"])
        assert not find_path_in_str(tmp_root_prefix / "bin", res["PATH"])
        assert find_path_in_str(tmp_root_prefix / "envs" / "xyz", res["PATH"])
        assert find_path_in_str(tmp_root_prefix / "envs" / "abc", res["PATH"])

        s = [
            f"{mamba_name} activate",
            f"{mamba_name} activate abc",
            f"{mamba_name} activate xyz --stack",
        ] + evars
        stdout, stderr = call(s)
        res = env_to_dict(stdout)
        assert find_path_in_str(tmp_root_prefix / "condabin", res["PATH"])
        assert not find_path_in_str(tmp_root_prefix / "bin", res["PATH"])
        assert find_path_in_str(tmp_root_prefix / "envs" / "xyz", res["PATH"])
        assert find_path_in_str(tmp_root_prefix / "envs" / "abc", res["PATH"])

        stdout, stderr = call(evars)
        res = env_to_dict(stdout)
        assert find_path_in_str(tmp_root_prefix / "condabin", res["PATH"])

        stdout, stderr = call([f"{mamba_name} deactivate"] + evars)
        res = env_to_dict(stdout)
        assert find_path_in_str(tmp_root_prefix / "condabin", res["PATH"])
        assert not find_path_in_str(tmp_root_prefix / "bin", res["PATH"])
        assert not find_path_in_str(tmp_root_prefix / "envs" / "xyz", res["PATH"])
        assert not find_path_in_str(tmp_root_prefix / "envs" / "abc", res["PATH"])


@pytest.mark.parametrize("interpreter", get_interpreters())
def test_activation_envvars(
    tmp_home,
    tmp_clean_env,
    winreg_value,
    tmp_root_prefix,
    tmp_path,
    interpreter,
):
    if interpreter not in valid_interpreters or (plat == "win" and interpreter == "bash"):
        pytest.skip(f"{interpreter} not available")

    umamba = helpers.get_umamba()
    mamba_name = Path(umamba).stem

    s = [f"{umamba} shell init -r {tmp_root_prefix}"]
    stdout, stderr = call_interpreter(s, tmp_path, interpreter)

    def call(s):
        return call_interpreter(s, tmp_path, interpreter, interactive=True)

    evars = extract_vars(["CONDA_PREFIX", "CONDA_SHLVL", "PATH"], interpreter)

    if interpreter == "cmd.exe":
        x = helpers.read_windows_registry(regkey)
        fp = Path(x[0][1:-1])
        assert fp.exists()

    if interpreter in ["bash", "zsh", "powershell", "cmd.exe"]:
        call([f"{mamba_name} create -n def -y"])

        stdout, stderr = call([f"{mamba_name} activate def"] + evars)
        res = env_to_dict(stdout)
        abc_prefix = pathlib.Path(res["CONDA_PREFIX"])

        state_file = abc_prefix / "conda-meta" / "state"

        # Python dicts are guaranteed to keep insertion order since 3.7,
        # so the following works fine!
        state_file.write_text(
            helpers.json.dumps(
                {
                    "env_vars": {
                        "test": "Test",
                        "HELLO": "world",
                        "WORKING": "/FINE/PATH/YAY",
                        "AAA": "last",
                    }
                }
            )
        )

        stdout, stderr = call(
            [f"{mamba_name} activate def"]
            + evars
            + extract_vars(["TEST", "HELLO", "WORKING", "AAA"], interpreter)
        )

        # assert that env vars are in the same order
        activation_script, stderr = call([f"{mamba_name} shell activate -s bash -n def"])
        idxs = []
        for el in ["TEST", "HELLO", "WORKING", "AAA"]:
            for idx, line in enumerate(activation_script.splitlines()):
                if line.startswith(f"export {el}="):
                    idxs.append(idx)
                    continue
        assert len(idxs) == 4

        # make sure that the order is correct
        assert idxs == sorted(idxs)

        res = env_to_dict(stdout)
        assert res["TEST"] == "Test"
        assert res["HELLO"] == "world"
        assert res["WORKING"] == "/FINE/PATH/YAY"
        assert res["AAA"] == "last"

        pkg_env_vars_d = abc_prefix / "etc" / "conda" / "env_vars.d"
        pkg_env_vars_d.mkdir(exist_ok=True, parents=True)

        j1 = {"PKG_ONE": "FANCY_ENV_VAR", "OVERLAP": "LOSE_AGAINST_PKG_TWO"}

        j2 = {
            "PKG_TWO": "SUPER_FANCY_ENV_VAR",
            "OVERLAP": "WINNER",
            "TEST": "LOSE_AGAINST_META_STATE",
        }

        (pkg_env_vars_d / "001-pkg-one.json").write_text(helpers.json.dumps(j1))
        (pkg_env_vars_d / "002-pkg-two.json").write_text(helpers.json.dumps(j2))

        activation_script, stderr = call([f"{mamba_name} shell activate -s bash -n def"])
        stdout, stderr = call(
            [f"{mamba_name} activate def"]
            + evars
            + extract_vars(
                [
                    "TEST",
                    "HELLO",
                    "WORKING",
                    "AAA",
                    "PKG_ONE",
                    "PKG_TWO",
                    "OVERLAP",
                ],
                interpreter,
            )
        )
        res = env_to_dict(stdout)

        assert res["HELLO"] == "world"
        assert res["WORKING"] == "/FINE/PATH/YAY"
        assert res["AAA"] == "last"
        assert res["PKG_ONE"] == "FANCY_ENV_VAR"
        assert res["PKG_TWO"] == "SUPER_FANCY_ENV_VAR"
        assert res["OVERLAP"] == "WINNER"
        assert res["TEST"] == "Test"


@pytest.mark.parametrize("interpreter", get_interpreters())
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_unicode_activation(
    tmp_home,
    winreg_value,
    tmp_root_prefix,
    tmp_path,
    interpreter,
):
    if interpreter not in valid_interpreters or (plat == "win" and interpreter == "bash"):
        pytest.skip(f"{interpreter} not available")

    umamba = helpers.get_umamba()
    mamba_name = Path(umamba).stem

    s = [f"{umamba} shell init -r {tmp_root_prefix}"]
    stdout, stderr = call_interpreter(s, tmp_path, interpreter)

    def call(s):
        return call_interpreter(s, tmp_path, interpreter, interactive=True)

    evars = extract_vars(["CONDA_PREFIX", "CONDA_SHLVL", "PATH"], interpreter)

    if interpreter == "cmd.exe":
        x = helpers.read_windows_registry(regkey)
        fp = Path(x[0][1:-1])
        assert fp.exists()

    if interpreter in ["bash", "zsh", "powershell", "cmd.exe"]:
        stdout, stderr = call(evars)

        s = [f"{umamba} --help"]
        stdout, stderr = call(s)

        s = [f"{mamba_name} activate"] + evars
        stdout, stderr = call(s)
        res = env_to_dict(stdout)

        assert "condabin" in res["PATH"]
        assert str(tmp_root_prefix) in res["PATH"]
        assert f"CONDA_PREFIX={tmp_root_prefix}" in stdout.splitlines()
        assert "CONDA_SHLVL=1" in stdout.splitlines()

        # throw with non-existent
        s = [f"{mamba_name} activate nonexistent"]
        if plat != "win":
            with pytest.raises(subprocess.CalledProcessError):
                stdout, stderr = call(s)

        u1 = "μυρτιὲς"
        u2 = "终过鬼门关"
        u3 = "some ™∞¢3 spaces §∞©ƒ√≈ç"
        s1 = [f"{mamba_name} create -n {u1} xtensor -y -c conda-forge"]
        s2 = [f"{mamba_name} create -n {u2} xtensor -y -c conda-forge"]
        if interpreter == "cmd.exe":
            s3 = [f'{mamba_name} create -n "{u3}" xtensor -y -c conda-forge']
        else:
            s3 = [f"{mamba_name} create -n '{u3}' xtensor -y -c conda-forge"]
        call(s1)
        call(s2)
        call(s3)

        for u in [u1, u2, u3]:
            install_prefix_root_dir = tmp_root_prefix / f"envs/{u}"
            assert (install_prefix_root_dir / "conda-meta").is_dir()
            assert (install_prefix_root_dir / "conda-meta/history").exists()
            if plat == "win":
                include_dir = install_prefix_root_dir / "Library/include"
            else:
                include_dir = install_prefix_root_dir / "include"
            assert include_dir.is_dir()
            helpers.PackageChecker("xtensor", install_prefix_root_dir).check_install_integrity()

        # unicode activation on win: todo
        if plat == "win":
            return

        s = [
            f"{mamba_name} activate",
            f"{mamba_name} activate {u1}",
            f"{mamba_name} activate {u2}",
        ] + evars
        stdout, stderr = call(s)
        res = env_to_dict(stdout)

        assert find_path_in_str(str(tmp_root_prefix / "condabin"), res["PATH"])
        assert not find_path_in_str(str(tmp_root_prefix / "bin"), res["PATH"])
        assert find_path_in_str(str(tmp_root_prefix / "envs" / u2), res["PATH"])
        assert not find_path_in_str(str(tmp_root_prefix / "envs" / u1), res["PATH"])

        s = [
            f"{mamba_name} activate",
            f"{mamba_name} activate {u1}",
            f"{mamba_name} activate {u2} --stack",
        ] + evars
        stdout, stderr = call(s)
        res = env_to_dict(stdout)
        assert find_path_in_str(str(tmp_root_prefix / "condabin"), res["PATH"])
        assert not find_path_in_str(str(tmp_root_prefix / "bin"), res["PATH"])
        assert find_path_in_str(str(tmp_root_prefix / "envs" / u1), res["PATH"])
        assert find_path_in_str(str(tmp_root_prefix / "envs" / u2), res["PATH"])

        s = [
            f"{mamba_name} activate",
            f"{mamba_name} activate '{u3}'",
        ] + evars
        stdout, stderr = call(s)
        res = env_to_dict(stdout)
        assert find_path_in_str(str(tmp_root_prefix / "condabin"), res["PATH"])
        assert not find_path_in_str(str(tmp_root_prefix / "bin"), res["PATH"])
        assert find_path_in_str(str(tmp_root_prefix / "envs" / u3), res["PATH"])


@pytest.mark.parametrize("interpreter", get_interpreters())
def test_activate_path(tmp_empty_env, tmp_env_name, interpreter, tmp_path):
    if interpreter not in valid_interpreters or (plat == "win" and interpreter == "bash"):
        pytest.skip(f"{interpreter} not available")

    # Activate env name
    res = helpers.shell("activate", tmp_env_name, "-s", interpreter)
    dict_res = env_to_dict(res, interpreter)

    assert any([str(tmp_empty_env) in p for p in dict_res.values()])

    # Activate path
    res = helpers.shell("activate", str(tmp_empty_env), "-s", interpreter)
    dict_res = env_to_dict(res, interpreter)
    assert any([str(tmp_empty_env) in p for p in dict_res.values()])

    # Activate path with home
    prefix_short = str(tmp_empty_env).replace(os.path.expanduser("~"), "~")
    res = helpers.shell("activate", prefix_short, "-s", interpreter)
    dict_res = env_to_dict(res, interpreter)
    assert any([str(tmp_empty_env) in p for p in dict_res.values()])


@pytest.mark.parametrize("conda_envs_x", ["CONDA_ENVS_DIRS", "CONDA_ENVS_PATH"])
@pytest.mark.parametrize("interpreter", get_interpreters())
def test_activate_envs_dirs(
    tmp_root_prefix: Path, interpreter, tmp_path: Path, conda_envs_x, monkeypatch
):
    """Activate an environment as the non leading entry in ``envs_dirs``."""
    env_name = "myenv"
    helpers.create("-p", tmp_path / env_name, "--offline", "--no-rc", no_dry_run=True)
    monkeypatch.setenv(conda_envs_x, f"{Path('/noperm')}{os.pathsep}{tmp_path}")
    res = helpers.shell("activate", env_name, "-s", interpreter)
    dict_res = env_to_dict(res, interpreter)
    assert any([env_name in p for p in dict_res.values()])


@pytest.fixture
def tmp_umamba():
    mamba_exe = helpers.get_umamba()
    shutil.copyfile(mamba_exe, mamba_exe + ".orig")

    yield mamba_exe

    shutil.move(mamba_exe + ".orig", mamba_exe)
    os.chmod(mamba_exe, 0o755)


@pytest.mark.skipif(
    "micromamba" not in Path(helpers.get_umamba()).stem,
    reason="micromamba-only test",
)
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("interpreter", get_self_update_interpreters())
def test_self_update(
    tmp_umamba,
    tmp_home,
    tmp_path,
    tmp_root_prefix,
    winreg_value,
    interpreter,
):
    mamba_exe = tmp_umamba
    mamba_name = Path(mamba_exe).stem

    shell_init = [
        f"{format_path(mamba_exe, interpreter)} shell init -s {interpreter} -r {format_path(tmp_root_prefix, interpreter)}"
    ]
    call_interpreter(shell_init, tmp_path, interpreter)

    if interpreter == "bash":
        assert (Path(tmp_root_prefix) / "etc" / "profile.d" / "mamba.sh").exists()

    extra_start_code = []
    if interpreter == "powershell":
        extra_start_code = [
            f'$Env:MAMBA_EXE="{mamba_exe}"',
            "$MambaModuleArgs = @{ChangePs1 = $True}",
            f'Import-Module "{tmp_root_prefix}\\condabin\\Mamba.psm1" -ArgumentList $MambaModuleArgs',
            "Remove-Variable MambaModuleArgs",
        ]
    elif interpreter == "bash":
        if plat == "linux":
            extra_start_code = ["source ~/.bashrc"]
        else:
            print(mamba_exe)
            extra_start_code = [
                f"source {PurePosixPath(tmp_home)}/.bash_profile",  # HOME from os.environ not acknowledged
                f"{mamba_name} info",
                "echo $MAMBA_ROOT_PREFIX",
                "echo $HOME",
                "ls ~",
                "echo $MAMBA_EXE",
            ]
    elif interpreter == "zsh":
        extra_start_code = ["source ~/.zshrc"]

    call_interpreter(
        extra_start_code + [f"{mamba_name} self-update --version 0.25.1 -c conda-forge"],
        tmp_path,
        interpreter,
        interactive=False,
    )

    assert Path(mamba_exe).exists()

    version = subprocess.check_output([mamba_exe, "--version"])
    assert version.decode("utf8").strip() == "0.25.1"

    assert not Path(mamba_exe + ".bkup").exists()

    shutil.copyfile(mamba_exe + ".orig", mamba_exe)
    os.chmod(mamba_exe, 0o755)
