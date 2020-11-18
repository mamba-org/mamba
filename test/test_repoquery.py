from mamba import repoquery


def test_depends():
    pool = repoquery.create_context(["conda-forge"], "linux-64", False)
    res = repoquery.depends("xtensor", pool)
    assert res["query"]["query"] == "xtensor"
    assert res["query"]["type"] == "depends"
    assert res["result"]["graph_roots"][0]["name"] == "xtensor"
    assert int(res["result"]["graph_roots"][0]["version"].split(".")[1]) >= 21
    pkgs = res["result"]["pkgs"]

    assert any(x["name"] == "libstdcxx-ng" for x in pkgs)
    assert pkgs[0]["subdir"] == "linux-64"


def test_whoneeds():
    pool = repoquery.create_context(["conda-forge"], "osx-64", False)
    res = repoquery.whoneeds("xtensor", pool)
    assert res["query"]["query"] == "xtensor"
    assert res["query"]["type"] == "whoneeds"

    assert res["result"]["graph_roots"] == ["xtensor"]

    pkgs = res["result"]["pkgs"]

    assert any(x["name"] == "xtensor-blas" for x in pkgs)
    assert pkgs[0]["subdir"] == "osx-64"


def test_search():
    pool = repoquery.create_context(["conda-forge"], "win-64", False)
    res = repoquery.search("xtensor*", pool)

    assert res["query"]["query"] == "xtensor*"
    assert res["query"]["type"] == "search"

    pkgs = res["result"]["pkgs"]

    assert any(x["name"] == "xtensor-blas" for x in pkgs)
    assert any(x["name"] == "xtensor" for x in pkgs)
    assert any(x["name"] == "xtensor-io" for x in pkgs)
    assert pkgs[0]["subdir"] == "win-64"

    res2 = repoquery.search("xtensor >=0.21", pool)

    assert all(int(x["version"].split(".")[1]) >= 21 for x in res2["result"]["pkgs"])
