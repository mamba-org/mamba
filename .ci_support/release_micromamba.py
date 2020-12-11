import os
import tarfile
import sys
import github


def _make_or_get_release(tag, repo):
    for rel in repo.get_releases():
        if rel.tag_name == tag:
            return rel

    # if we get here no release was found
    return repo.create_git_release(tag, tag, "", prerelease=True)


def _make_or_upload_asset(path, rel):
    name = os.path.basename(path)
    for ast in rel.get_assets():
        if ast.name == name:
            return ast

    # if we get here, we upload the asset
    return rel.upload_asset(
        path,
        content_type="application/x-bzip2",
    )


if __name__ == "__main__":
    tag = sys.argv[1]
    if sys.platform.startswith("win"):
        platform = "win"
    elif sys.platform == "darwin":
        platform = "osx"
    else:
        platform = "linux"

    print("making release for tag:", tag, flush=True)

    gh = github.Github(os.environ["GH_TOKEN"])
    repo = gh.get_repo(os.environ["GITHUB_REPOSITORY"])

    rel = _make_or_get_release(tag, repo)

    with tarfile.open(f"micromamba-{platform}.tar.bz2", "w:bz2") as tf:
        tf.add("micromamba", arcname="bin/micromamba")

    print("uploading the asset...", flush=True)
    ast = _make_or_upload_asset(f"micromamba-{platform}.tar.bz2", rel)

    # if we get here, we mark the release as not a pre-release
    rel.update_release(tag, "", prerelease=False)
