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
