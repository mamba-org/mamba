import json
import os
import random
import string
import subprocess
from pathlib import Path

use_offline = False
channel = ["-c", "conda-forge"]


def get_umamba():
    if os.getenv("TEST_MAMBA_EXE"):
        umamba = os.getenv("TEST_MAMBA_EXE")
    else:
        cwd = os.getcwd()
        umamba = os.path.join(cwd, "build", "micromamba")
    if not os.path.exists(umamba):
        print("MICROMAMBA NOT FOUND!")
    return umamba


def random_string(N=10):
    return "".join(random.choices(string.ascii_uppercase + string.digits, k=N))


def install(*args):
    umamba = get_umamba()
    cmd = [umamba, "install"] + [arg for arg in args if arg] + channel
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


def update(*args):
    umamba = get_umamba()
    cmd = [umamba, "update", "-y", "--no-rc"] + [arg for arg in args if arg] + channel
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


def get_pkg(n, f=None):
    root_prefix = os.getenv("MAMBA_ROOT_PREFIX")
    if f:
        return Path(os.path.join(root_prefix, "pkgs", n, f))
    else:
        return Path(os.path.join(root_prefix, "pkgs", n))


def get_concrete_pkg_info(env, pkg_name):
    with open(os.path.join(env, "conda-meta", pkg_name + ".json")) as fi:
        return json.load(fi)
