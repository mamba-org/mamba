.. _commands_micromamba/config/remove:

``remove``
============

Usage: ``micromamba config remove [OPTIONS] [remove...]``


**Positionals:**

``remove TEXT``

Remove a configuration value from a list key. This removes all instances of the value.

**Options:**

``-h,--help``

Print this help message and exit.

``--system``

Excludes: ``--env --file``
Set configuration on system's rc file.

``--env``

Excludes: ``--system --file``
Set configuration on env's rc file.

``--file TEXT``

Excludes: ``--system --env``
Set configuration on system's rc file.
