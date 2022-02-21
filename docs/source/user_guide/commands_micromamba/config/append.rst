.. _commands_micromamba/config/append:

``append``
============

Usage: ``micromamba config append [OPTIONS] [specs...]``


**Positionals:**

``specs [TEXT,TEXT] ... REQUIRED``

Add value at the end of a configurable sequence.


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
