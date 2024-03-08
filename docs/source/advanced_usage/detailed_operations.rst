.. _detailed_operations:

Detailed operations
===================

Overview
--------

| This section explains what are the main operations in details.

Please have a look at the :ref:`more concepts<more_concepts>` section, the following documentation relies heavily on it.


.. _detailed_install:

Install
-------

The ``install`` operation is using a ``package specification`` to add/install additional packages to a :ref:`target prefix<prefix>`.

The workflow for that operation is:

.. mermaid::
   :align: center

   %%{init: {'themeVariables':{'edgeLabelBackground':'white'}}}%%
   graph TD
      style start fill:#00000000,stroke:#00000000,color:#00000000;
      config[Compute configuration fa:fa-link]
      fetch_index[Fetch repositories index fa:fa-unlink]
      solve[Solving fa:fa-unlink]
      fetch_pkgs[Fetch packages fa:fa-unlink]
      extract[Extract tarballs fa:fa-unlink]
      link[Link fa:fa-link]
      stop([Stop])

      start-->|User spec|config;
      config-->fetch_index;
      fetch_index-->solve;
      solve-->fetch_pkgs;
      fetch_pkgs-->extract;
      extract-->link;
      link-->stop;

      click config href "../user_guide/configuration.html"
      click link href "./more_concepts.html#linking"

See also: :ref:`package tarball<tarball>`


The package installation process
--------------------------------

When a package gets installed, several steps are executed:

- the package is downloaded and placed into the ``$ROOT_PREFIX/pkgs`` folder
- the package is extracted
- the package is "linked" from the ``pkgs`` folder into the final destination

When the package is linked to the final destination (for example, some newly created environment), most files are "hard"-linked. That means, there is no copy of the file created. This saves a considerable amount of disk-space when re-using the same package in multiple environments.

However, sometimes a text-replacement is necessary. This is the case when a file contains a reference to the prefix. On Unix systems a very long prefix is used during build (you might have seen something like ``_h_123123_placeholder_placeholder_placeholder...``). This very long prefix is stored in the metadata of the package. When a file contains this prefix, the file is copied into the target prefix and the prefix it is replaced. In text files, this is done via a simple string replacement, and in binary files the string is padded with ``\0`` bytes (to keep the same total length of the file). Binary replacement on Windows is usually not necessary because dynamic library loading on Windows follows different rules.

When installing a ``noarch: python`` package, the installation process will compile all ``.py``-files into Python bytecode (``.pyc``) files.

All installed files are later referenced in the ``$TARGET_PREFIX/conda-meta/mypkg-version-build.json`` file, to facilitate the removal (e.g. when upgrading or removing a package).

If the package contains a ``menu/*.json`` entry that follows the spec introduced by ``menuinst``, a start-menu entry is created on Windows. This is currently not implemented on Linux or macOS but that might change in the future.
