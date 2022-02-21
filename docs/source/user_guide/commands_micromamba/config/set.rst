.. _commands_micromamba/config/set:

``set``
============

Usage: ``micromamba config set [OPTIONS] [set_value]``


**Positionals:**

``set_value TEXT``

Set configuration value on rc file.

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
