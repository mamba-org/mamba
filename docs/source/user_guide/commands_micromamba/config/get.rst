.. _commands_micromamba/config/get:

``get``
============

Usage: ``micromamba config get [OPTIONS] [get_value]``


**Positionals:**

``get_value TEXT``

Display configuration value from rc file.

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
