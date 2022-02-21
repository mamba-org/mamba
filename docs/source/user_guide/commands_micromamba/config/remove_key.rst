.. _commands_micromamba/config/remove_key:

``remove-key``
============

Usage: ``micromamba config remove-key [OPTIONS] [remove_key]``


**Positionals:**

``remove_key TEXT``

Remove a configuration key and its values.

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
