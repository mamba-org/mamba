.. _commands_micromamba/config/list:

``list``
========

Usage: ``micromamba config list [OPTIONS] [configs...]``


**Positionals:**

``configs TEXT ...``

Configuration keys.


**Options:**

``-h,--help``

Print this help message and exit.

``-l,--long-descriptions``

Display configs long descriptions.

``-g,--groups``

Display configs groups.

``-s,--sources``

Display all configs sources.

``-a,--all``

Display all rc configurable configs.

``-d,--descriptions``

Display configs descriptions.


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
