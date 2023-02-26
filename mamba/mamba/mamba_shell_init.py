import os
from os.path import join

from conda.activate import native_path_to_unix
from conda.base.constants import COMPATIBLE_SHELLS
from conda.base.context import context
from conda.common.compat import on_mac, on_win
from conda.core.initialize import initialize, initialize_dev, make_initialize_plan

HERE = os.path.dirname(__file__)
SHELL_TEMPLATES = join(HERE, "shell_templates")

MAMBA_SNIPPET_SH = """
if [ -f "{mamba_source_path}" ]; then
    . "{mamba_source_path}"
fi
"""
MAMBA_SNIPPET_FISH = """
if test -f "{mamba_source_path}"
    source "{mamba_source_path}"
end
"""


def add_mamba_to_rcfile(file, conda_prefix):
    def check_init_block(lines, start_i, prefix):
        i = start_i
        while i >= 0:
            if ">>> conda initialize >>>" in lines[i]:
                break
            i -= 1
        x = lines[i:start_i]
        return prefix in "".join(x)

    with open(file, "r") as fi:
        current_content = fi.readlines()

    if file.endswith(".fish"):
        snippet = MAMBA_SNIPPET_FISH
        mamba_source_path = native_path_to_unix(
            join(conda_prefix, "etc", "fish", "conf.d", "mamba.fish")
        )
    else:
        snippet = MAMBA_SNIPPET_SH
        mamba_source_path = native_path_to_unix(
            join(conda_prefix, "etc", "profile.d", "mamba.sh")
        )
    new_content = []
    for i, line in enumerate(current_content):
        if line.startswith("# <<< conda initialize <<<"):
            if check_init_block(current_content, i, native_path_to_unix(conda_prefix)):
                new_content.append(snippet.format(mamba_source_path=mamba_source_path))
        new_content.append(line)

    with open(file, "w") as fo:
        fo.write("".join(new_content))

    print(f"Added mamba to {file}")
    return True


def shell_init(args):
    if args.all:
        selected_shells = COMPATIBLE_SHELLS
    else:
        selected_shells = tuple(args.shells)

    if not selected_shells:
        if on_win:
            selected_shells = ("cmd.exe", "powershell")
        if on_mac:
            selected_shells = ("bash", "zsh")
        else:
            selected_shells = ("bash",)

    if args.dev:
        assert (
            len(selected_shells) == 1
        ), "--dev can only handle one shell at a time right now"
        shell = selected_shells[0]
        return initialize_dev(shell)

    else:
        if hasattr(args, "no_user"):
            # this seems to be conda < 23.1.0
            # Here, we use the old behavior even though the --no-user flag
            # doesn't behave as desired, see:
            # https://github.com/conda/conda/issues/11948
            for_user = args.user
            if not (args.install and args.user and args.system):
                for_user = True
            if args.no_user:
                for_user = False
        else:
            for_user = args.user and not args.system

        anaconda_prompt = on_win and args.anaconda_prompt
        exit_code = initialize(
            context.conda_prefix,
            selected_shells,
            for_user,
            args.system,
            anaconda_prompt,
            args.reverse,
        )

    changed = False
    if not exit_code and not args.reverse:
        plan = make_initialize_plan(
            context.conda_prefix,
            selected_shells,
            for_user,
            args.system,
            anaconda_prompt,
            args.reverse,
        )
        for el in plan:
            if el.get("function") in (
                "init_sh_user",
                "init_sh_system",
                "init_fish_user",
            ):
                args = el.get("kwargs")
                target_path = args["target_path"]
                conda_prefix = args["conda_prefix"]
                add_mamba_to_rcfile(target_path, conda_prefix)
                changed = True
    if changed:
        print(
            "\n==> For changes to take effect, close "
            "and re-open your current shell. <==\n"
        )
    return exit_code
