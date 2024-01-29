import glob
import json
import os
import shutil
import subprocess

from . import helpers


def constructor(*args, default_channel=True, no_rc=True, no_dry_run=False):
    umamba = helpers.get_umamba()
    cmd = [umamba, "constructor"] + [arg for arg in args if arg]

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
        return res.decode()

    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


class TestInstall:
    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = helpers.random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + helpers.random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)
    new_cache = os.path.join(root_prefix, "pkgs")

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.prefix

        # speed-up the tests
        os.environ["CONDA_PKGS_DIRS"] = TestInstall.new_cache
        os.makedirs(TestInstall.new_cache, exist_ok=True)
        root_pkgs = glob.glob(os.path.join(TestInstall.current_root_prefix, "pkgs", "x*.tar.bz2"))
        urls = []

        for pkg in root_pkgs:
            shutil.copy(pkg, TestInstall.new_cache)
            urls.append(
                "http://testurl.com/conda-forge/linux-64/" + os.path.basename(pkg) + "#123412341234"
            )

        cls.pkgs = [os.path.basename(pkg) for pkg in root_pkgs]
        with open(os.path.join(TestInstall.new_cache, "urls"), "w") as furls:
            furls.write("\n".join(urls))

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.current_prefix
        shutil.rmtree(TestInstall.root_prefix)

    @classmethod
    def teardown_method(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.prefix

    def test_extract_pkgs(self):
        constructor("--prefix", TestInstall.root_prefix, "--extract-conda-pkgs")

        for pkg in self.pkgs:
            extracted_pkg = os.path.join(TestInstall.root_prefix, "pkgs", pkg.rsplit(".tar.bz2")[0])
            with open(os.path.join(extracted_pkg, "info", "repodata_record.json")) as rr:
                repodata_record = json.load(rr)
            with open(os.path.join(extracted_pkg, "info", "index.json")) as ri:
                index = json.load(ri)
            assert repodata_record["fn"] == pkg
            assert repodata_record["md5"] == "123412341234"
            assert repodata_record["url"] == "http://testurl.com/conda-forge/linux-64/" + pkg
            assert repodata_record["depends"] == index["depends"]
