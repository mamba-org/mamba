import json
import os
import platform
import random
import string
import subprocess
from enum import Enum
from pathlib import Path

import yaml


class DryRun(Enum):
    OFF = "OFF"
    DRY = "DRY"
    ULTRA_DRY = "ULTRA_DRY"


use_offline = False
channel = ["-c", "conda-forge"]
dry_run_tests = DryRun(
    os.environ["MAMBA_DRY_RUN_TESTS"]
    if ("MAMBA_DRY_RUN_TESTS" in os.environ)
    else "OFF"
)

MAMBA_NO_PREFIX_CHECK = 1 << 0
MAMBA_ALLOW_EXISTING_PREFIX = 1 << 1
MAMBA_ALLOW_MISSING_PREFIX = 1 << 2
MAMBA_ALLOW_NOT_ENV_PREFIX = 1 << 3
MAMBA_EXPECT_EXISTING_PREFIX = 1 << 4

MAMBA_NOT_ALLOW_EXISTING_PREFIX = 0
MAMBA_NOT_ALLOW_MISSING_PREFIX = 0
MAMBA_NOT_ALLOW_NOT_ENV_PREFIX = 0
MAMBA_NOT_EXPECT_EXISTING_PREFIX = 0

if platform.system() == "Windows":
    xtensor_hpp = "Library/include/xtensor/xtensor.hpp"
    xsimd_hpp = "Library/include/xsimd/xsimd.hpp"
else:
    xtensor_hpp = "include/xtensor/xtensor.hpp"
    xsimd_hpp = "include/xsimd/xsimd.hpp"


def get_umamba(cwd=os.getcwd()):
    if os.getenv("TEST_MAMBA_EXE"):
        umamba = os.getenv("TEST_MAMBA_EXE")
    else:
        if platform.system() == "Windows":
            umamba_bin = "micromamba.exe"
        else:
            umamba_bin = "micromamba"
        umamba = os.path.join(cwd, "build", umamba_bin)
    if not Path(umamba).exists():
        print("MICROMAMBA NOT FOUND!")
    return umamba


def random_string(N=10):
    return "".join(random.choices(string.ascii_uppercase + string.digits, k=N))


def shell(*args, cwd=os.getcwd()):
    umamba = get_umamba(cwd=cwd)
    cmd = [umamba, "shell"] + [arg for arg in args if arg]

    if "--print-config-only" in args:
        cmd += ["--debug"]

    res = subprocess.check_output(cmd)
    if "--json" in args:
        try:
            j = json.loads(res)
            return j
        except json.decoder.JSONDecodeError as e:
            print(f"Error when loading JSON output from {res}")
            raise (e)
    if "--print-config-only" in args:
        return yaml.load(res, Loader=yaml.FullLoader)
    return res.decode()


def info(*args):
    umamba = get_umamba()
    cmd = [umamba, "info"] + [arg for arg in args if arg]
    res = subprocess.check_output(cmd)
    if "--json" in args:
        try:
            j = json.loads(res)
            return j
        except json.decoder.JSONDecodeError as e:
            print(f"Error when loading JSON output from {res}")
            raise (e)
    return res.decode()


def install(*args, default_channel=True, no_rc=True, no_dry_run=False):
    umamba = get_umamba()
    cmd = [umamba, "install", "-y"] + [arg for arg in args if arg]

    if "--print-config-only" in args:
        cmd += ["--debug"]
    if default_channel:
        cmd += channel
    if no_rc:
        cmd += ["--no-rc"]
    if use_offline:
        cmd += ["--offline"]
    if (dry_run_tests == DryRun.DRY) and "--dry-run" not in args and not no_dry_run:
        cmd += ["--dry-run"]

    res = subprocess.check_output(cmd)
    if "--json" in args:
        try:
            j = json.loads(res)
            return j
        except:
            print(res.decode())
            return
    if "--print-config-only" in args:
        return yaml.load(res, Loader=yaml.FullLoader)
    return res.decode()


def create(*args, default_channel=True, no_rc=True, no_dry_run=False, always_yes=True):
    umamba = get_umamba()
    cmd = [umamba, "create"] + [arg for arg in args if arg]

    if "--print-config-only" in args:
        cmd += ["--debug"]
    if always_yes:
        cmd += ["-y"]
    if default_channel:
        cmd += channel
    if no_rc:
        cmd += ["--no-rc"]
    if use_offline:
        cmd += ["--offline"]
    if (dry_run_tests == DryRun.DRY) and "--dry-run" not in args and not no_dry_run:
        cmd += ["--dry-run"]

    try:
        res = subprocess.check_output(cmd)
        if "--json" in args:
            j = json.loads(res)
            return j
        if "--print-config-only" in args:
            return yaml.load(res, Loader=yaml.FullLoader)
        return res.decode()
    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


def remove(*args, no_dry_run=False):
    umamba = get_umamba()
    cmd = [umamba, "remove", "-y"] + [arg for arg in args if arg]

    if "--print-config-only" in args:
        cmd += ["--debug"]
    if (dry_run_tests == DryRun.DRY) and "--dry-run" not in args and not no_dry_run:
        cmd += ["--dry-run"]

    try:
        res = subprocess.check_output(cmd)
        if "--json" in args:
            j = json.loads(res)
            return j
        if "--print-config-only" in args:
            return yaml.load(res, Loader=yaml.FullLoader)
        return res.decode()
    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


def update(*args, default_channel=True, no_rc=True, no_dry_run=False):
    umamba = get_umamba()
    cmd = [umamba, "update", "-y"] + [arg for arg in args if arg]
    if use_offline:
        cmd += ["--offline"]
    if no_rc:
        cmd += ["--no-rc"]
    if default_channel:
        cmd += channel
    if (dry_run_tests == DryRun.DRY) and "--dry-run" not in args and not no_dry_run:
        cmd += ["--dry-run"]

    try:
        res = subprocess.check_output(cmd)
        if "--json" in args:
            try:
                j = json.loads(res)
                return j
            except json.decoder.JSONDecodeError as e:
                print(f"Error when loading JSON output from {res}")
                raise (e)
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)

        return res.decode()
    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


def umamba_list(*args):
    umamba = get_umamba()

    cmd = [umamba, "list"] + [arg for arg in args if arg]
    res = subprocess.check_output(cmd)

    if "--json" in args:
        j = json.loads(res)
        return j

    return res.decode()


def get_concrete_pkg(t, needle):
    pkgs = t["actions"]["LINK"]
    pkg_name = None
    for p in pkgs:
        if p["name"] == needle:
            return f"{p['name']}-{p['version']}-{p['build_string']}"
    raise RuntimeError("Package not found in transaction")


def get_env(n, f=None):
    root_prefix = os.getenv("MAMBA_ROOT_PREFIX")
    if f:
        return Path(os.path.join(root_prefix, "envs", n, f))
    else:
        return Path(os.path.join(root_prefix, "envs", n))


def get_pkg(n, f=None, root_prefix=os.getenv("MAMBA_ROOT_PREFIX")):
    if f:
        return Path(os.path.join(root_prefix, "pkgs", n, f))
    else:
        return Path(os.path.join(root_prefix, "pkgs", n))


def get_tarball(n):
    root_prefix = os.getenv("MAMBA_ROOT_PREFIX")
    return Path(os.path.join(root_prefix, "pkgs", n + ".tar.bz2"))


def get_concrete_pkg_info(env, pkg_name):
    with open(os.path.join(env, "conda-meta", pkg_name + ".json")) as fi:
        return json.load(fi)
