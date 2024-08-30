import os
import platform
import shutil
import subprocess
import time
import uuid
from pathlib import Path

import pytest
import yaml


def get_lines(std_pipe):
    """Generator that yields lines from a standard pipe as they are printed."""
    for line in iter(std_pipe.readline, ""):
        yield line


class Shell:
    def __init__(self, shell_type):
        self.process = subprocess.Popen(
            [shell_type],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            universal_newlines=True,
        )
        self.sentinel = "__command_done__"
        self.echo_sentinel = "echo " + self.sentinel

        if platform.system() == "Windows":
            if shell_type == "cmd.exe":
                self.execute("@ECHO OFF")
                self.execute("set CONDA_SHLVL=0")
            elif shell_type == "powershell":
                self.execute(
                    "conda shell.powershell hook | Out-String | Invoke-Expression"
                )
                self.execute("$env:CONDA_SHLVL=0")
        else:
            self.execute("export CONDA_SHLVL=0")
            self.execute("CONDA_BASE=$(conda info --base)")
            self.execute("source $CONDA_BASE/etc/profile.d/conda.sh")

    def execute(self, commands):
        if type(commands) == str:
            commands = [commands, self.echo_sentinel]
        else:
            commands = list(commands)
            commands.append(self.echo_sentinel)

        for cmd in commands:
            if not cmd.endswith("\n"):
                cmd += "\n"
            self.process.stdin.write(cmd)
            self.process.stdin.flush()

        out = []
        start = time.time()
        wait_for_sentinel = True
        while wait_for_sentinel:
            for line in get_lines(self.process.stdout):
                if self.sentinel != line[:-1]:
                    if all([c not in line[:-1] for c in commands]) and line[:-1]:
                        print(line, end="")
                        out.append(line[:-1])
                else:
                    wait_for_sentinel = False
                    break

            if (time.time() - start) > 5:
                break

        return out

    def conda(self, command):
        return self.execute(" ".join(("conda", command)))

    def mamba(self, command):
        return self.execute(" ".join(("mamba", command)))

    def exit(self):
        self.process.kill()


class Environment:
    def __init__(self, shell_type):
        self.shell = Shell(shell_type)
        self.name = "env_" + str(uuid.uuid1())

        self.shell.conda("create -q -y -n " + self.name)
        self.shell.conda(f"activate {self.name}")

    def __enter__(self):
        return self.shell

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.shell.conda("deactivate")
        self.shell.conda(f"remove -q -y --name {self.name} --all")
        self.shell.exit()


def platform_shells():
    if platform.system() == "Windows":
        return ["powershell", "cmd.exe"]
    else:
        return ["bash"]


def get_glibc_version():
    try:
        output = subprocess.check_output(["ldd", "--version"])
    except:
        return
    output.splitlines()
    version = output.splitlines()[0].split()[-1]
    return version.decode("ascii")


def run(exe, channels, package):
    cmd = [
        exe,
        "create",
        "-n",
        "xxx",
        "--override-channels",
        "--strict-channel-priority",
        "--dry-run",
    ]
    for channel in channels:
        cmd += ["-c", os.path.abspath(os.path.join(*channel))]
    cmd.append(package)
    subprocess.run(cmd, check=True)


def run_mamba_conda(channels, package):
    run("conda", channels, package)
    run("mamba", channels, package)


@pytest.fixture
def config_file(request):
    file_loc = Path.home() / ".condarc"
    old_config_file = None
    if file_loc.exists():
        old_config_file = file_loc.rename(Path.home() / ".condarc.bkup")

    with open(file_loc, "w") as fo:
        yaml.dump(request.param, fo)

    yield file_loc

    if old_config_file:
        file_loc.unlink()
        old_config_file.rename(file_loc)


def add_glibc_virtual_package():
    version = get_glibc_version()
    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, "channel_a/linux-64/repodata.tpl")) as f:
        repodata = f.read()
    with open(os.path.join(here, "channel_a/linux-64/repodata.json"), "w") as f:
        if version is not None:
            glibc_placeholder = ', "__glibc=' + version + '"'
        else:
            glibc_placeholder = ""
        repodata = repodata.replace("GLIBC_PLACEHOLDER", glibc_placeholder)
        f.write(repodata)


def copy_channels_osx():
    here = os.path.dirname(os.path.abspath(__file__))
    # In CI, macos-latest can be arm64 or not
    for arch in ["osx-64", "osx-arm64"]:
        for channel in ["a", "b"]:
            if not os.path.exists(os.path.join(here, f"channel_{channel}/{arch}")):
                shutil.copytree(
                    os.path.join(here, f"channel_{channel}/linux-64"),
                    os.path.join(here, f"channel_{channel}/{arch}"),
                )
                with open(
                    os.path.join(here, f"channel_{channel}/{arch}/repodata.json")
                ) as f:
                    repodata = f.read()
                with open(
                    os.path.join(here, f"channel_{channel}/{arch}/repodata.json"), "w"
                ) as f:
                    if arch == "osx-64":
                        repodata = repodata.replace("linux", "osx")
                    else:
                        repodata = repodata.replace("linux-", "osx-arm")
                    f.write(repodata)
