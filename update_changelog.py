# Script to update `CHANGELOG.md` in order to release any of the mamba packages

# Steps:

# 1. Run this script to update the root `CHANGELOG.md` file by giving the date of
# the last release as input (cf. last date shown at the top of the file for reference)
# or any other starting date that may be relevant for the release,
# and the release version to be made.

# 2. If you are happy with the changes, run `releaser.py` to update the versions and
# corresponding nested `CHANGELOG.md` files.

# N.B If the release is to be a pre-release (alpha,...), the versions should not be updated in `.py` and `.h` files.
# Only the `CHANGELOG.md` files should be modified.
# If otherwise, please revert the corresponding files if modified by the script.

# 3. Follow the steps described in the `releaser.py` output.

from datetime import date

import json
import re
import subprocess
import argparse
from version_scheme import version_info


def validate_date(date_str):
    try:
        date.fromisoformat(date_str)
    except ValueError:
        raise ValueError("Incorrect date format, should be YYYY-MM-DD")


def subprocess_run(*args: str, **kwargs) -> str:
    """Execute a command in a subprocess while properly capturing stderr in exceptions."""
    try:
        p = subprocess.run(args, capture_output=True, check=True, **kwargs)
    except subprocess.CalledProcessError as e:
        print(f"Command {args} failed with stderr: {e.stderr.decode()}")
        print(f"Command {args} failed with stdout: {e.stdout.decode()}")
        raise e
    return p.stdout


def append_to_file(ctgr_name, prs, out_file):
    out_file.write(f"\n{ctgr_name}:\n\n")
    for pr in prs:
        # Author
        pr_author_cmd = f"gh pr view {pr} --json author"
        author_login = dict(json.loads(subprocess_run(*pr_author_cmd.split()).decode("utf-8")))[
            "author"
        ]["login"]
        # Title
        pr_title_cmd = f"gh pr view {pr} --json title"
        title = dict(json.loads(subprocess_run(*pr_title_cmd.split()).decode("utf-8")))["title"]
        # URL
        pr_url_cmd = f"gh pr view {pr} --json url"
        url = dict(json.loads(subprocess_run(*pr_url_cmd.split()).decode("utf-8")))["url"]
        # Files
        # Use a different command with graphql allowing pagination
        # (since number of files retrieved with 'gh pr view {pr} files' is limited to 100)
        # cf. https://github.com/cli/cli/issues/5368
        graphql_files_cmd = f"gh api graphql -f query='query($owner: String!, $repo: String!, $pr: Int!, $endCursor: String) {{repository(owner: $owner, name: $repo) {{pullRequest(number: $pr) {{files(first: 100, after: $endCursor) {{pageInfo{{ hasNextPage, endCursor }} nodes {{path}}}}}}}}}}' -F owner='mamba-org' -F repo='mamba' -F pr={pr} --paginate --jq '.data.repository.pullRequest.files.nodes.[].path'"
        files = subprocess.run(
            graphql_files_cmd, shell=True, capture_output=True, text=True
        ).stdout.split("\n")

        ref_mamba_pkgs = ["libmamba/", "libmambapy/", "micromamba/"]
        concerned_pkgs = set()
        for f in files:
            for ref_pkg in ref_mamba_pkgs:
                if f.startswith(ref_pkg):
                    concerned_pkgs.add(ref_pkg)

        if (sorted(ref_mamba_pkgs) == sorted(concerned_pkgs)) or (len(concerned_pkgs) == 0):
            concerned_pkgs = ["all/"]
        # Write in file
        out_file.write(
            "- [{}] {} by @{} in {}\n".format(
                (", ".join([pkg[:-1] for pkg in concerned_pkgs])), title, author_login, url
            )
        )


def main():
    cli_parser = argparse.ArgumentParser("changelog updater")
    cli_parser.add_argument(
        "--from_date",
        "-d",
        help="Starting date of commits to be included in the release in the format YYYY-MM-DD.",
    )
    cli_parser.add_argument("--version", "-v", help="Name of the version to be released.")
    args = cli_parser.parse_args()

    commits_starting_date = None
    if args.from_date is not None:
        commits_starting_date = args.from_date
    else:
        commits_starting_date = input(
            "Enter the starting date of commits to be included in the release in the format YYYY-MM-DD: "
        )

    validate_date(commits_starting_date)
    release_version = None
    if args.version is not None:
        release_version = args.version
    else:
        release_version = input("Enter the version to be released: ")

    release_version = version_info(release_version)

    # Get commits to include in the release
    log_cmd = "git log --since=" + commits_starting_date
    commits = subprocess_run(*log_cmd.split()).decode("utf-8")

    # Create the regular expression pattern
    opening_char = "(#"
    closing_char = ")"
    pattern = re.compile(re.escape(opening_char) + "(.*?)" + re.escape(closing_char))

    # Get the PRs numbers
    prs_nbrs = re.findall(pattern, commits)

    # Make three lists to categorize PRs: "Enhancements", "Bug fixes" and "CI fixes and doc"
    enhancements_prs = []  # release::enhancements
    bug_fixes_prs = []  # release::bug_fixes
    ci_docs_prs = []  # release::ci_docs

    for pr in prs_nbrs:
        # Get labels
        pr_labels_cmd = f"gh pr view {pr} --json labels"
        labels = dict(json.loads(subprocess_run(*pr_labels_cmd.split()).decode("utf-8")))["labels"]
        nb_rls_lbls_types = 0
        label = ""
        for lab in labels:
            if lab["name"].startswith("release::"):
                nb_rls_lbls_types = nb_rls_lbls_types + 1
                label = lab["name"]

        # Only one release label should be set
        if nb_rls_lbls_types == 0:
            raise ValueError(f"No release label is set for PR #{pr}")
        elif nb_rls_lbls_types > 1:
            raise ValueError(
                "Only one release label should be set. PR #{} has {} labels.".format(
                    pr, nb_rls_lbls_types
                )
            )

        # Dispatch PRs with their corresponding label
        if label == "release::enhancements":
            enhancements_prs.append(pr)
        elif label == "release::bug_fixes":
            bug_fixes_prs.append(pr)
        elif label == "release::ci_docs":
            ci_docs_prs.append(pr)
        else:
            raise ValueError(f"Unknown release label {label} for PR #{pr}")

    with open("CHANGELOG.md", "r+") as changelog_file:
        # Make sure we're appending at the beginning of the file
        content_to_restore = changelog_file.read()
        changelog_file.seek(0)

        # Append new info
        # Release date and version
        changelog_file.write("{}\n".format(date.today().strftime("%Y.%m.%d")))
        changelog_file.write("==========\n")
        changelog_file.write(
            "\nRelease: {0} (libmamba, mamba, micromamba, libmambapy)\n".format(release_version)
        )
        # PRs info
        if enhancements_prs:
            append_to_file("Enhancements", enhancements_prs, changelog_file)
        if bug_fixes_prs:
            append_to_file("Bug fixes", bug_fixes_prs, changelog_file)
        if ci_docs_prs:
            append_to_file("CI fixes and doc", ci_docs_prs, changelog_file)

        # Write back old content of CHANGELOG file
        changelog_file.write("\n" + content_to_restore)

    print(
        "'CHANGELOG.md' was successfully updated.\nPlease run 'releaser.py' if you agree with the changes applied."
    )


if __name__ == "__main__":
    main()
