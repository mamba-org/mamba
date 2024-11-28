# Script to release any of the mamba packages
# Please refer to `update_changelog.py` for more info about the release process

import copy
import datetime
import re

template = {"version": None, "changes": []}

templates = {
    "libmamba": "libmamba/include/mamba/version.hpp.tmpl",
    "micromamba": "micromamba/src/version.hpp.tmpl",
    "libmambapy": "libmambapy/src/libmambapy/version.py.tmpl",
}


def apply_changelog(name, version, changes):
    if "-" in version:
        raise ValueError(
            "'{}' is not a valid version name : `-` is reserved for another usage in conda packages version names".format(
                version
            )
        )

    VALID_VERSION_PRERELEASE_TYPES = ("alpha", "beta")
    version_fields = version.split(".")
    version_fields_count = len(version_fields)
    if version_fields_count < 3:
        raise ValueError(
            "'{}' is not a valid version name :  valid version scheme contains 3 or more dots-separated fields, the pre-release name starting with the 4th field (valid examples: 1.2.3, 0.1.2.alpha3, 0.1.2.alpha.3)".format(
                version
            )
        )

    version_major = version_fields[0]
    version_minor = version_fields[1]
    version_patch = version_fields[2]
    version_prerelease = ""
    if version_fields_count > 3:
        # we assume here that all the additional dot-separated values are part of the pre-release name
        version_prerelease = ".".join(version_fields[3:])

    version_errors = []

    if not version_major.isdigit():
        version_errors.append("'{}' is not a valid major version number".format(version_major))
    if not version_minor.isdigit():
        version_errors.append("'{}' is not a valid minor version number".format(version_minor))
    if not version_patch.isdigit():
        version_errors.append("'{}' is not a valid patch version number".format(version_patch))

    if version_prerelease != "" and not version_prerelease.startswith(
        VALID_VERSION_PRERELEASE_TYPES
    ):
        version_errors.append(
            "'{}' is not a valid pre-release name, pre-release names must start with either : {} ".format(
                version_prerelease, VALID_VERSION_PRERELEASE_TYPES
            )
        )

    if len(version_errors) > 0:
        error_message = "'{}' is not a valid version name:".format(version)
        for error in version_errors:
            error_message += "\n - {}".format(error)
        hint = "examples of valid versions: 1.2.3, 0.1.2, 1.2.3.alpha0, 1.2.3.beta1, 3.4.5.beta.2"
        error_message += "\n{}".format(hint)
        raise ValueError(error_message)

    def template_substitute(contents):
        x = contents.replace("{{ version_major }}", version_major)
        x = x.replace("{{ version_minor }}", version_minor)
        x = x.replace("{{ version_patch }}", version_patch)
        x = x.replace("{{ version_is_prerelease }}", "1" if version_prerelease else "0")
        x = x.replace("{{ version_prerelease_name }}", version_prerelease)
        x = x.replace("{{ version_name }}", version)
        return x

    if name in templates:
        template = templates[name]
        with open(template, "r") as fi:
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
    with open(cl_file, "r") as fi:
        prev_cl = fi.read()
    with open(cl_file, "w") as fo:
        fo.write(res + prev_cl)


def commands(changes):
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

    print(f"git tag {date_stamp}")
    for c in changes:
        print(f"git tag {c}-{changes[c]['version']}")


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
    with open("CHANGELOG.md", "r") as fi:
        contents = fi.readlines()

    for idx, line in enumerate(contents):
        if line.startswith("====="):
            release_start = idx + 1
            break

    brackets_re = re.compile(r"\[(.*?)\]")

    # section with groups, heading + items
    sections = []
    in_section = False

    contents = contents[release_start:]
    for idx, c in enumerate(contents):
        if c.startswith("Releases"):
            releases = [x.strip() for x in c[len("Releases: ") :].split(",")]
            for r in releases:
                rsplit = r.split()
                changes[rsplit[0].strip()] = copy.deepcopy(template)
                changes[rsplit[0].strip()]["version"] = rsplit[1].strip()
            continue

        if contents[idx + 1].startswith("===="):
            break

        if c.strip() == "" or c[0] == "-":
            in_section = False

        if c.strip() == "":
            continue

        if c[0] != "-":
            if not in_section:
                sections.append(Section())
                in_section = True
            sections[-1].text += c

        if m := re.search(brackets_re, c):
            if in_section:
                sections[-1].applies_to = [x.strip() for x in m.groups(1)[0].split(",")]
            else:
                sections[-1].items.append(Item())
                sections[-1].items[-1].text = c[m.end() :].strip()
                sections[-1].items[-1].applies_to = [x.strip() for x in m.groups(1)[0].split(",")]

        else:
            if c.startswith(" "):
                if in_section:
                    sections[-1].text += " " + c.strip()
                else:
                    sections[-1].items[-1].text += c.strip()
            else:
                if not in_section:
                    sections[-1].items.append(Item())
                    sections[-1].items[-1].text = c.strip()
                    sections[-1].items[-1].applies_to = ["all"]

    for c in changes:
        populate_changes(c, sections, changes)

    for el in changes:
        apply_changelog(el, changes[el]["version"], changes[el]["changes"])

    commands(changes)


if __name__ == "__main__":
    main()
