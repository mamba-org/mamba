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
    # Note that the pre-release is essential since otherwise the GitHub links to the
    # latest release will point to partial releases, breaking any workflows that
    # depend on those links.
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
    print("platform machine:", platform.machine(), flush=True)
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
    if subdir.startswith("win"):
        dest_pth = f"{name}\\Library\\bin\\micromamba.exe"
        src_pth = "micromamba.exe"
        dest_exe = name + ".exe"
    else:
        dest_pth = f"{name}/bin/micromamba"
        src_pth = "micromamba"
        dest_exe = name

    with tarfile.open(f"{name}.tar.bz2", "w:bz2") as tf:
        tf.add(src_pth, arcname=dest_pth)

    print("uploading the tar.bz2 asset...", flush=True)
    ast = _make_or_upload_asset(f"{name}.tar.bz2", rel)

    print("uploading the exe asset...", flush=True)
    ast = _make_or_upload_asset(src_pth, rel, name=dest_exe)
