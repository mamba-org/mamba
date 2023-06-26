.. _artifacts_verification:

Artifacts verification
======================

Overview
--------

| Artifacts verification is a general description to describe the security steps realized by the package manager.

3 verifications are implemented in Mamba, on:

- repositories packages index (experimental)
- packages tarballs, fetched from package repo
- packages files, expanded from tarballs


.. _repodata_verification:

Repodata
--------

| The verification of :ref:`repodata<repodata>` files is under active development based on `The Update Framework specification <https://theupdateframework.github.io/specification/latest/>`_.

The goal is to ensure that a :ref:`package tarball<tarball>` metadata are correct (including size and checksums).

It relies on multiple (asymmetric) cryptographic keys to:

- define one or multiple trusted public keys for a given package (also called target in that context)
- add to the ``repodata`` files one or more (public key, signature) pair for each package tarball metadata

Further documentation will come soon.


.. _tarball_verification:

Package tarball
---------------

Assuming a valid :ref:`repodata<repodata>` (see the previous :ref:`repodata verification<repodata_verification>`), :ref:`package tarball<tarball>` metadata are used to check if a tarball is valid or not after fetching it from a repository.


.. _files_verification:

Package files
-------------

| All the package files are listed in a ``paths.json`` file index extracted from the :ref:`package tarball<tarball>` with files themselves.

This index also contains metadata such as the size and checksum (SHA-256) of each file of the package.

When a package has already been extracted during a previous operation, it can be directly re-used.
The files sizes are nevertheless verified to be sure that they match package definition.
It prevents from alteration of its content (manual editing of a file, etc.).

SHA-256 checksum verification can be additionally performed using ``extra safety checks`` configuration.

By default, Mamba will only emit a warning if one of those 2 checks (file size and checksum) are failing.
You can also configure a different policy:

- ignore
- warn
- throw

.. note::
    After fetching a package tarball from a repo, its size and checksums are already verified (see the previous :ref:`package tarball verification<tarball_verification>`). There is no need to perform additional checks on each file.
