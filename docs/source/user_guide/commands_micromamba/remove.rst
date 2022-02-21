.. _commands_micromamba/remove:

``remove``
=============

Usage: ``micromamba remove [OPTIONS] [specs...]``


**Positionals:**

``specs TEXT ...``

Specs to remove from the environment.


**Options:**

``-h,--help``

Print this help message and exit.

``-a,--all``

Remove all packages in the environment.

``-f,--force``

Force removal of package (note: consistency of environment is not guaranteed!)

``--prune,--no-prune{false}``

Prune dependencies (default).


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


**Prefix options:**

``-r,--root-prefix TEXT``

Path to the root prefix.

``-p,--prefix TEXT``

Path to the target prefix.

``-n,--name TEXT``

Name of the target prefix.
