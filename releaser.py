# Script to release any of the mamba packages
# Please refer to `update_changelog.py` for more info about the release process

import copy
import datetime
import re
from version_scheme import version_info

template = {"version": None, "changes": []}

templates = {
    "libmamba": "libmamba/include/mamba/version.hpp.tmpl",
    "micromamba": "micromamba/src/version.hpp.tmpl",
    "libmambapy": "libmambapy/src/libmambapy/version.py.tmpl",
}


def apply_changelog(name, version, changes):

    def template_substitute(contents):
        x = contents.replace("{{ version_major }}", version.major)
        x = x.replace("{{ version_minor }}", version.minor)
        x = x.replace("{{ version_patch }}", version.patch)
        x = x.replace("{{ version_is_prerelease }}", "1" if version.pre_release else "0")
        x = x.replace("{{ version_prerelease_name }}", version.pre_release)
        x = x.replace("{{ version_name }}", version.name)
        return x

    if name in templates:
        template = templates[name]
        with open(template) as fi:
            final = template_substitute(fi.read())
        with open(template[: -len(".tmpl")], "w") as fo:
            fo.write(final)

    # version has been processed, we can now produce the changes
    res = ""
    today = datetime.date.today()
    fmt_today = today.strftime("%B %d, %Y")

    header_line = f"{name} {version} ({fmt_today})"
    res += f"{header_line}\n{'=' * len(header_line)}\n\n"

    for idx, c in enumerate(changes):
        if c.startswith("-"):
            if idx > 0 and not changes[idx - 1].startswith("- "):
                res += f"\n{c}\n"
            else:
                res += f"{c}\n"
        else:
            res += f"{c}\n"
    res += "\n"

    cl_file = name + "/CHANGELOG.md"
    with open(cl_file) as fi:
        prev_cl = fi.read()
    with open(cl_file, "w") as fo:
        fo.write(res + prev_cl)


def commands(release_version, changes):
    commit_msg = ", ".join([f"{x} {changes[x]['version']}" for x in changes])

    today = datetime.date.today()
    date_stamp = today.strftime("%Y.%m.%d")

    files_to_commit = ""
    for c in changes:
        files_to_commit += f"    {c}/CHANGELOG.md \\\n"
        files_to_commit += f"    {templates[c][:-len('.tmpl')]} \\\n"
    files_to_commit = files_to_commit[:-3]

    for c in changes:
        files_to_commit += f"    {templates[c][:-len('.tmpl')]} \\\n"
    print("\n\n--- REVERT ---\n\n")
    print(f"git checkout origin/main -- \\\n{files_to_commit[:-3]}\n\n")

    print("\n\n--- COMMIT ---\n\n")
    print("pre-commit run --all")
    print("git diff")
    files_to_commit += "    CHANGELOG.md \\\n"
    print(f"git commit -m 'release {commit_msg}' \\\n{files_to_commit[:-3]}")

    print(f"git tag {release_version}")


class Section:
    def __init__(self):
        self.items = []
        self.applies_to = ["all"]
        self.text = ""


class Item:
    def __init__(self):
        self.applies_to = ["all"]
        self.text = ""


def populate_changes(name, sections, changes):
    el = changes[name]

    def applies(x):
        return "all" in x or name in x

    for s in sections:
        s_applies = applies(s.applies_to)
        if s_applies and len(s.items):
            s_applies = any(applies(i.applies_to) for i in s.items)

        if s_applies:
            if s != sections[0]:
                el["changes"].append("\n" + s.text.strip())
            else:
                el["changes"].append(s.text.strip())
            for i in s.items:
                if applies(i.applies_to):
                    el["changes"].append(f"- {i.text.strip()}")


def main():
    changes = {}
    with open("CHANGELOG.md") as fi:
        contents = fi.readlines()

    for idx, line in enumerate(contents):
        if line.startswith("====="):
            release_start = idx + 1
            break

    brackets_re = re.compile(r"\[(.*?)\]")

    # section with groups, heading + items
    sections = []
    in_section = False

    release_version = None
    contents = contents[release_start:]
    for idx, line in enumerate(contents):
        if line.startswith("Release"):
            release_re = re.compile(r"Release*:\s+(\d\.\d\.\d[\.\w]*)\s+\(([\w,\s]+)\)\s*")
            if matches := re.search(release_re, line):
                release_version = matches.group(1)
                projects = matches.group(2).replace(',', " ").split()
                print("projects: {}".format(projects))
                for project in projects:
                    # because `micromamba` is now the name of the `mamba` project's directory, we ignore it
                    if project != 'mamba':
                        changes[project] = copy.deepcopy(template)
                        changes[project]["version"] = release_version
            continue

        if contents[idx + 1].startswith("===="):
            break

        if line.strip() == "" or line[0] == "-":
            in_section = False

        if line.strip() == "":
            continue

        if line[0] != "-":
            if not in_section:
                sections.append(Section())
                in_section = True
            sections[-1].text += line

        if m := re.search(brackets_re, line):
            if in_section:
                sections[-1].applies_to = [x.strip() for x in m.groups(1)[0].split(",")]
            else:
                sections[-1].items.append(Item())
                sections[-1].items[-1].text = line[m.end() :].strip()
                sections[-1].items[-1].applies_to = [x.strip() for x in m.groups(1)[0].split(",")]

        else:
            if line.startswith(" "):
                if in_section:
                    sections[-1].text += " " + line.strip()
                else:
                    sections[-1].items[-1].text += line.strip()
            else:
                if not in_section:
                    sections[-1].items.append(Item())
                    sections[-1].items[-1].text = line.strip()
                    sections[-1].items[-1].applies_to = ["all"]

    if release_version is None:
        raise ValueError("Version to release not found - use `update_changelog.py` to specify it")

    release_version = version_info(release_version)

    for project_name in changes:
        populate_changes(project_name, sections, changes)

    for project_name in changes:
        apply_changelog(project_name, release_version, changes[project_name]["changes"])

    commands(release_version, changes)


if __name__ == "__main__":
    main()
