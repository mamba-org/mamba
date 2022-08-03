import sys
import subprocess


def generate_string_docs(commands_list, micromamba_cmd):
    for c in commands_list:

        out = subprocess.check_output([micromamba_cmd, c, "--help"])

        opts = out.decode("utf-8")

        with open("./source/user_guide/commands_micromamba/" + c + ".rst", "w") as f:
            f.write(".. _commands_micromamba/" + c + ":" + "\n\n")
            f.write("``" + c + "``" + "\n")
            f.write("=" * (len(c) + 4) + "\n")

            print("generating ", c)

            for line in opts.splitlines():
                aux = line

                if len(line) == 0:
                    continue

                # Name of session
                if line[0] != " " and ":" in line:
                    if line.startswith("Usage:"):
                        aux = aux.replace("Usage: ", "")
                        f.write("**Usage:**" + " ``" + aux + "`` \n")
                        aux = ""
                        continue
                    if line.startswith("Subcommands"):
                        break
                    f.write("\n**" + line[:-1] + ":**\n\n")
                    aux = aux.replace(line[:-1] + ":", "")

                else:
                    split = line.split()
                    for o in split:
                        # Flags
                        if o[0] == "-":
                            f.write("``" + o + "`` ")
                            aux = aux.replace(o, "")
                        # General arguments
                        if o.isupper():
                            f.write(o + " ")
                            aux = aux.replace(o, "")
                        # Arguments with OR statement
                        if o.isupper() and o == "OR":
                            f.write(aux + " ")
                            aux = ""

                # Flags' text string
                aux = " ".join(aux.split())
                if len(aux) > 0:
                    f.write("\n\n" + aux + "\n\n")


if __name__ == "__main__":

    micromamba_cmd = sys.argv[1] if len(sys.argv) > 1 else "micromamba"

    commands_list = [
        "activate",
        "deactivate",
        "shell",
        "create",
        "install",
        "update",
        "remove",
        "list",
        "config",
        "info",
        "clean",
        "constructor",
        "env",
        "repoquery",
    ]

    generate_string_docs(commands_list, micromamba_cmd)

    # Generate sub-files for config command
    with open("./source/user_guide/commands_micromamba/config.rst", "a") as f:
        config_subcommands = [
            "**Subcommands:**\n\n",
            "- :ref:`commands_micromamba/config/list`\n",
            "- :ref:`commands_micromamba/config/sources`\n",
            "- :ref:`commands_micromamba/config/describe`\n",
            "- :ref:`commands_micromamba/config/prepend`\n",
            "- :ref:`commands_micromamba/config/append`\n",
            "- :ref:`commands_micromamba/config/remove_key`\n",
            "- :ref:`commands_micromamba/config/remove`\n",
            "- :ref:`commands_micromamba/config/set`\n",
        ]
        f.writelines(config_subcommands)

    config_commands_list = [
        "config/list",
        "config/sources",
        "config/describe",
        "config/prepend",
        "config/append",
        "config/remove_key",
        "config/remove",
        "config/set",
    ]

    generate_string_docs(config_commands_list, micromamba_cmd)
