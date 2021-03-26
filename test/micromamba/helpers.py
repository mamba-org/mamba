import json
import os
import platform
import random
import string
import subprocess
from pathlib import Path

use_offline = False
channel = ["-c", "conda-forge"]


if platform.system() == "Windows":
    xtensor_hpp = "Library/include/xtensor/xtensor.hpp"
    xsimd_hpp = "Library/include/xsimd/xsimd.hpp"
else:
    xtensor_hpp = "include/xtensor/xtensor.hpp"
    xsimd_hpp = "include/xsimd/xsimd.hpp"


def get_umamba():
    if os.getenv("TEST_MAMBA_EXE"):
        umamba = os.getenv("TEST_MAMBA_EXE")
    else:
        cwd = os.getcwd()
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


def shell(*args):
    umamba = get_umamba()
    cmd = [umamba, "shell"] + [arg for arg in args if arg]
    res = subprocess.check_output(cmd)
    if "--json" in args:
        try:
            j = json.loads(res)
            return j
        except json.decoder.JSONDecodeError as e:
            print(f"Error when loading JSON output from {res}")
            raise (e)
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


def install(*args):
    umamba = get_umamba()
    cmd = [umamba, "install", "-y", "--no-rc"] + [arg for arg in args if arg] + channel
    if use_offline:
        cmd += ["--offline"]
    res = subprocess.check_output(cmd)
    if "--json" in args:
        try:
            j = json.loads(res)
            return j
        except:
            print(res.decode())
            return
    return res.decode()


def remove(*args):
    umamba = get_umamba()
    cmd = [umamba, "remove"] + [arg for arg in args if arg]
    res = subprocess.check_output(cmd)
    if "--json" in args:
        try:
            j = json.loads(res)
            return j
        except:
            print(res.decode())
            return
    return res.decode()


def create(*args):
    umamba = get_umamba()
    cmd = [umamba, "create", "-y"] + [arg for arg in args if arg] + channel
    if use_offline:
        cmd += ["--offline"]

    try:
        res = subprocess.check_output(cmd)
        if "--json" in args:
            j = json.loads(res)
            return j
        return res.decode()

    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


def remove(*args):
    umamba = get_umamba()
    cmd = [umamba, "remove", "-y"] + [arg for arg in args if arg]

    try:
        res = subprocess.check_output(cmd)
        if "--json" in args:
            j = json.loads(res)
            return j
        return res.decode()

    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


def update(*args):
    umamba = get_umamba()
    cmd = [umamba, "update", "-y", "--no-rc"] + [arg for arg in args if arg] + channel
    if use_offline:
        cmd += ["--offline"]

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
