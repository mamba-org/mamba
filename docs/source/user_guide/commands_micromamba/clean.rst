.. _commands_micromamba/clean:

``clean``
=========

Usage: ``micromamba clean [OPTIONS]``

**Options:**

``-h, --help``

Print a help message about clean and exit.

``-a, --all``

Remove index cache, lock files, unused cache packages, and tarballs.

``-i,--index-cache``

Remove index cache.

``-p,--packages``

Remove unused packages from writable package caches.

``-t,--tarballs``

Remove cached package tarballs.

``-l,--locks``

Remove lock files from caches

``--trash``

Remove *.mamba_trash files from all environments

``-f,--force-pkgs-dirs``

Remove *all* writable package caches. This option is not included with the --all flags.


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