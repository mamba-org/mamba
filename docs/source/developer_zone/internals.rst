Internals of mamba
==================

Mamba comes with a C++ core (for speed and efficiency), and a clean Python API on top. The core of mamba uses:

- ``libsolv`` to solve dependencies (the same library used in RedHat dnf and other Linux package managers)
- ``curl`` for reliable and fast downloads
- ``libarchive`` to extract the packages
