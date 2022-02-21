.. _commands_micromamba/package:

``package``
===========

Usage: ``micromamba package [OPTIONS] [SUBCOMMAND]``


**Options:**

``-h,--help``

Print this help message and exit.

**Subcommands:**

``extract``

Extract an archive to a destination folder.

Usage: ``micromamba package [ARCHIVE] [PATH]``

``compress``

Compress a folder to a ``.tar.bz2`` or a ``.conda`` file, e.g. `myfile-3.1-0.tar.bz2` or `myfile-3.1-0.conda`.

Aditionally, ``compress`` has the following subcommand:

- ``-c,--compression-level``: Compression level from 0-9 (tar.bz2, default is 9), and 1-22 (conda, default is 15)

``transmute``

Convert a ``.tar.bz2`` file to ``.conda`` a package.

Aditionally, ``transmute`` has the following subcommand:

- ``-c,--compression-level``: Compression level from 0-9 (tar.bz2, default is 9), and 1-22 (conda, default is 15)