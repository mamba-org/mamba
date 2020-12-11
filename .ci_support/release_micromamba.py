import os
import tarfile
import sys
import github
import platform


def _make_or_get_release(tag, repo):
    for rel in repo.get_releases():
        if rel.tag_name == tag:
            return rel

    # if we get here no release was found
    return repo.create_git_release(tag, tag, "", prerelease=True)


def _make_or_upload_asset(path, rel, name=None):
    ast_name = os.path.basename(path)
    for ast in rel.get_assets():
        if ast.name == ast_name:
            return ast

    # if we get here, we upload the asset
    if name is not None:
        return rel.upload_asset(
            path,
            name=name,
        )
    else:
        return rel.upload_asset(
            path,
            content_type="application/x-bzip2",
        )


if __name__ == "__main__":
    tag = sys.argv[1]
    if platform.machine() == "x86_64":
        plat = "64"
    else:
        raise ValueError("could not find a valid platform!")

    if sys.platform.startswith("win"):
        subdir = f"win-{plat}"
    elif sys.platform == "darwin":
        subdir = f"osx-{plat}"
    else:
        subdir = f"linux-{plat}"

    print("making release for tag:", tag, flush=True)

    gh = github.Github(os.environ["GH_TOKEN"])
    repo = gh.get_repo(os.environ["GITHUB_REPOSITORY"])

    rel = _make_or_get_release(tag, repo)

    name = f"micromamba-{subdir}"
    if platform == "win":
        dest_pth = f"{name}\\Library\\bin\\micromamba.exe"
        src_pth = "micromamba.exe"
        dest_exe = name + "_" + subdir + ".exe"
    else:
        dest_pth = f"{name}/bin/micromamba"
        src_pth = "micromamba"
        dest_exe = name + "_" + subdir

    with tarfile.open(f"{name}.tar.bz2", "w:bz2") as tf:
        tf.add(src_pth, arcname=dest_pth)

    print("uploading the tar.bz2 asset...", flush=True)
    ast = _make_or_upload_asset(f"micromamba-{platform}.tar.bz2", rel)

    print("uploading the exe asset...", flush=True)
    ast = _make_or_upload_asset(src_pth, rel, name=dest_exe)

    # if we get here, we mark the release as not a pre-release
    rel.update_release(tag, "", prerelease=False)
