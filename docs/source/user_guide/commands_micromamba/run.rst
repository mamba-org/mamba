.. _commands_micromamba/run:

``run``
===========

Usage: ``micromamba run [OPTIONS]``


**Options:**

``-h,--help``

Print this help message and exit.

``-a, --attach TEXT``

Attach to stdin, stdout and/or stderr. "-a" for disabling stream redirection.

``--cwd TEXT``

Current working directory for command to run in. Defaults to ``cwd``.

``-d,--detach``

Detach process from terminal.

``--clean-env``

Start with a clean environment.

``-e,--env TEXT ...``

Add env vars with ``-e ENVVAR`` or ``-e ENVVAR=VALUE``.


**Prefix options:**

``-r,--root-prefix TEXT``

Path to the root prefix.

``-p,--prefix TEXT``

Path to the target prefix.

``-n,--name TEXT``

Name of the target prefix.
