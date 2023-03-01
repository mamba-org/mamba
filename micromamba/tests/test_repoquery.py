import os
import shutil

import pytest

from .helpers import create, get_env, random_string, umamba_repoquery


class TestRepoquery:
    env_name = random_string()
    root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    prefix = os.path.join(root_prefix, "envs", env_name)

    @classmethod
    def setup_class(cls):
        create(
            "yaml=0.2.5",
            "pyyaml=6.0",
            "-c",
            "conda-forge",
            "-n",
            TestRepoquery.env_name,
            "--json",
            no_dry_run=True,
        )
        os.environ["CONDA_PREFIX"] = TestRepoquery.prefix

    @classmethod
    def teardown_class(cls):
        os.environ["CONDA_PREFIX"] = TestRepoquery.current_prefix
        shutil.rmtree(get_env(TestRepoquery.env_name))

    # Testing `depends`
    def test_depends(self):
        res = umamba_repoquery("depends", "yaml=0.2.5", "--json")

        assert res["query"]["query"] == "yaml =0.2.5*"
        assert res["query"]["type"] == "depends"
        assert res["result"]["graph_roots"][0]["channel"] == "conda-forge"
        assert res["result"]["graph_roots"][0]["name"] == "yaml"
        assert res["result"]["graph_roots"][0]["version"] == "0.2.5"

        pkgs = res["result"]["pkgs"]
        assert any(x["name"] == "libgcc-ng" for x in pkgs)
        assert any(x["name"] == "yaml" for x in pkgs)

    def test_depends_remote(self):
        res = umamba_repoquery("depends", "yaml", "--use-local=0")

        assert 'No entries matching "yaml" found' in res
        assert (
            "yaml may not be installed. Try giving a channel with '-c,--channel' option for remote repoquery\n"
            in res
        )

    def test_depends_not_installed(self):
        res = umamba_repoquery("depends", "xtensor")
        assert 'No entries matching "xtensor" found' in res
        assert (
            "xtensor may not be installed. Try giving a channel with '-c,--channel' option for remote repoquery\n"
            in res
        )

    def test_depends_not_installed_with_channel(self):
        res = umamba_repoquery(
            "depends", "-c", "conda-forge", "xtensor=0.24.5", "--json"
        )

        assert res["query"]["query"] == "xtensor =0.24.5*"
        assert res["query"]["type"] == "depends"
        assert "conda-forge" in res["result"]["graph_roots"][0]["channel"]
        assert res["result"]["graph_roots"][0]["name"] == "xtensor"
        assert res["result"]["graph_roots"][0]["version"] == "0.24.5"

        pkgs = res["result"]["pkgs"]
        assert any(x["name"] == "libgcc-ng" for x in pkgs)
        assert any(x["name"] == "libstdcxx-ng" for x in pkgs)
        assert any(x["name"] == "xtensor" for x in pkgs)
        assert any(x["name"] == "xtl" for x in pkgs)

    def test_depends_recursive(self):
        res = umamba_repoquery(
            "depends", "-c", "conda-forge", "xtensor=0.24.5", "--recursive"
        )

        assert "libzlib" in res
        assert "llvm-openmp" in res

    def test_depends_tree(self):
        res = umamba_repoquery(
            "depends", "-c", "conda-forge", "xtensor=0.24.5", "--tree"
        )

        assert "libzlib" in res
        assert "llvm-openmp" in res

    # Testing `whoneeds`
    def test_whoneeds(self):
        res = umamba_repoquery("whoneeds", "yaml", "--json")

        assert res["query"]["query"] == "yaml"
        assert res["query"]["type"] == "whoneeds"
        assert res["result"]["pkgs"][0]["channel"] == "conda-forge"
        assert res["result"]["pkgs"][0]["name"] == "pyyaml"
        assert res["result"]["pkgs"][0]["version"] == "6.0"

    def test_whoneeds_remote(self):
        res = umamba_repoquery("whoneeds", "yaml", "--use-local=0")

        assert 'No entries matching "yaml" found' in res
        assert (
            "yaml may not be installed. Try giving a channel with '-c,--channel' option for remote repoquery\n"
            in res
        )

    def test_whoneeds_not_installed(self):
        res = umamba_repoquery("whoneeds", "xtensor")
        assert 'No entries matching "xtensor" found' in res
        assert (
            "xtensor may not be installed. Try giving a channel with '-c,--channel' option for remote repoquery\n"
            in res
        )

    def test_whoneeds_not_installed_with_channel(self):
        res = umamba_repoquery(
            "whoneeds", "-c", "conda-forge", "xtensor=0.24.5", "--json"
        )

        assert res["query"]["query"] == "xtensor =0.24.5*"
        assert res["query"]["type"] == "whoneeds"

        pkgs = res["result"]["pkgs"]
        assert all("conda-forge" in x["channel"] for x in pkgs)
        assert any(x["name"] == "cppcolormap" for x in pkgs)
        assert any(x["name"] == "pyxtensor" for x in pkgs)
        assert any(x["name"] == "qpot" for x in pkgs)

    def test_whoneeds_tree(self):
        res = umamba_repoquery(
            "whoneeds", "-c", "conda-forge", "xtensor=0.24.5", "--tree"
        )

        assert "cppcolormap" in res
        assert "pyxtensor" in res
        assert "qpot" in res

    # Testing `search`
    def test_remote_search_installed_pkg(self):
        res = umamba_repoquery("search", "yaml")

        assert 'No entries matching "yaml" found' in res
        assert (
            "Channels may not be configured. Try giving a channel with '-c,--channel' option, or use `--use-local=1` to search for installed packages."
            in res
        )

    def test_local_search_installed_pkg(self):
        res = umamba_repoquery("search", "yaml", "--use-local=1", "--json")

        assert res["query"]["query"] == "yaml"
        assert res["query"]["type"] == "search"
        assert res["result"]["pkgs"][0]["channel"] == "conda-forge"
        assert res["result"]["pkgs"][0]["name"] == "yaml"
        assert res["result"]["pkgs"][0]["version"] == "0.2.5"

    def test_remote_search_not_installed_pkg(self):
        res = umamba_repoquery(
            "search", "-c", "conda-forge", "xtensor=0.24.5", "--json"
        )

        assert res["query"]["query"] == "xtensor =0.24.5*"
        assert res["query"]["type"] == "search"
        assert "conda-forge" in res["result"]["pkgs"][0]["channel"]
        assert res["result"]["pkgs"][0]["name"] == "xtensor"
        assert res["result"]["pkgs"][0]["version"] == "0.24.5"
