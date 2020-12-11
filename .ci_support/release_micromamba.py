import os
import sys

import github


def _make_or_get_release(tag, repo):
    for rel in repo.get_releases():
        if rel.tag_name == tag:
            return rel

    # if we get here no release was found
    # Note that the pre-release is essential since otherwise the GitHub links to the
    # latest release will point to partial releases, breaking any workflows that
    # depend on those links.
    return repo.create_git_release(tag, tag, "", prerelease=True)


if __name__ == "__main__":
    tag = sys.argv[1]

    print("making release for tag:", tag, flush=True)

    gh = github.Github(os.environ["GH_TOKEN"])
    repo = gh.get_repo(os.environ["GITHUB_REPOSITORY"])

    rel = _make_or_get_release(tag, repo)

    rel.update_release(tag, "", prerelease=False)
