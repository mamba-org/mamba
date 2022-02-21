.. _commands_micromamba/shell:

``shell``
=========

Usage: ``micromamba shell [OPTIONS] action [prefix]``


**Positionals:**

``action TEXT:{init,hook,activate,deactivate,reactivate} REQUIRED``

The action to complete.

``prefix TEXT``

The root prefix to configure (for init and hook), and the prefix to activate for activate, either by name or by path.


**Options:**

``-h,--help``

Print a help message and exit.

``-s,--shell TEXT:{bash,cmd.exe,fish,posix,powershell,xonsh,zsh}``

A shell type.

``--stack``

Stack the environment being activated.

``-p,-n,--prefix,--name TEXT``

The root prefix to configure (for init and hook), and the prefix to activate for activate, either by name or by path.


**Configuration options:**

``--rc-file TEXT``

Paths to the configuration files to use.

``--no-rc``

Disable the use of configuration files.

``--no-env``

Disable the use of environment variables.


**Global options:**

``-v,--verbose``

Set verbosity (higher verbosity with multiple -v, e.g. -vvv).

``--log-level TEXT:{critical,error,warning,info,debug,trace,off}``

Set the log level.

``-q,--quiet``

Set quiet mode (print less output).

``-y,--yes``

Automatically answer yes on prompted questions.

``--json``

Report all output as json.

``--offline``

Force use cached repodata.

``--dry-run``

Only display what would have been done.

``--experimental``

Enable experimental features.