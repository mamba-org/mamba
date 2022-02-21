.. _commands_micromamba/list:

``list``
========

Usage: ``micromamba list [OPTIONS] [regex]``


**Positionals:**

``regex TEXT``

List only packages matching a regular expression.

**Options:**

``-h,--help``

Print this help message and exit.

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
