.. _troubleshooting:

Troubleshooting
===============

Please use the official installer
---------------------------------

Please make sure that you use the :ref:`official Mambaforge installer <mamba-install>` to install Mamba.
Other installation methods are not supported.

Mamba should be installed to the ``base`` environment
-----------------------------------------------------

Installing Mamba to an environment other than ``base`` is not supported. Mamba must be installed in the ``base`` environment alongside Conda, and no other packages may be installed into ``base``.

.. _base_packages:

No other packages should be installed to ``base``
-------------------------------------------------

Installing packages other than Conda and Mamba into the ``base`` environment is not supported. Mamba must live in the same environment as Conda, and Conda does not support having packages other than Conda itself and its dependencies in ``base``.

.. _defaults_channels:

Using the ``defaults`` channels
-------------------------------

It is **not recommended** to use the
`Anaconda default channels <https://docs.anaconda.com/free/anaconda/reference/default-repositories/>`_:

- ``pkgs/main``
- ``pkgs/r`` / ``R``
- ``msys2``
- ``defaults`` (which includes all of the above)

Please note that we won't be able to help resolving any problems you might face with the Anaconda channels.

To check if you have any Anaconda default channels in your configuration, use::

    $ mamba info
    ...
    channel URLs : https://repo.anaconda.com/pkgs/... # BAD!
                   ...
                   https://conda.anaconda.org/conda-forge/...
    ...

Please change your configuration to use only ``conda-forge`` using one of the following methods.

Disable the default channels in your install commands::

  mamba install --override-channels ...

Or your :file:`environment.yml` file:

.. code-block:: yaml

  name: ...
  channels:
    - ...
    - nodefaults

Or in your :file:`~/.condarc` file:

.. code-block:: yaml

  ...
  channels:
    - ...
    - defaults  # BAD! Remove this if it exists.
    - nodefaults

Mixing the ``defaults`` and ``conda-forge`` channels
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The `Anaconda default channels <https://docs.anaconda.com/free/anaconda/reference/default-repositories/>`_
are **incompatible** with conda-forge.

Using the default and ``conda-forge`` channels at the same time is not supported and will lead to broken environments:

.. code-block:: yaml

  # NOT supported!
  channels:
    - conda-forge
    - defaults

Mamba broken after Conda update
-------------------------------

Mamba sometimes stops working if you update to a very recent version of Conda.
Please downgrade to the latest working a version and file a bug report in the Mamba bug tracker
if the problem has not been reported yet.

Mamba or Micromamba broken after an update
------------------------------------------

While we make our best effort to keep backward compatibility, it is not impossible that an update
breaks the current installation.
The following actions can be tried:

- Reinitializing your shell with ``micromamba shell reinit``.
- Deleting the package cache (``"package cache"`` entries in ``micromamba info``)

libmamba.so.2: undefined symbol ...
-----------------------------------

See :ref:`defaults_channels`.

Windows long paths
------------------

Windows historically limits many Win32 paths to ``MAX_PATH`` (260 characters). Starting in
Windows 10 version 1607, this restriction can be lifted for applications that opt in.


Long paths support has to be activated
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Microsoft documents the two required conditions in `Enable long paths in Windows 10, version 1607, and later
<https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry#enable-long-paths-in-windows-10-version-1607-and-later>`_:

1. The system registry value ``LongPathsEnabled`` must be set to ``1`` under
   ``HKLM\SYSTEM\CurrentControlSet\Control\FileSystem``.
2. The application manifest must declare ``longPathAware`` (official micromamba builds ship with
   ``longpath.manifest``).

``libmamba`` checks both conditions at runtime via ``are_long_paths_enabled()`` and logs a
diagnostic when linking or filesystem operations fail while long paths are disabled.

Set the ``LongPathsEnabled`` registry value to ``1`` under
``HKLM\SYSTEM\CurrentControlSet\Control\FileSystem``, or enable **Enable Win32 long paths** in
Group Policy (**Computer Configuration > Administrative Templates > System > Filesystem**).
Microsoft's guide linked above describes both approaches. A reboot or sign-out may be required
for running processes to pick up the registry change.


cmd.exe does not support calls to long prefixes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

While ``cmd.exe`` shell support long paths prefixing for directory operations such as ``dir``, it doesn't allow to call an executable or a batch file located at a long path prefix.

Thus, the following cases will fail:

- completely

  - calling executables located at long prefixes
  - installation of packages with pre/post linking or activation ``.bat`` scripts

- partially

  - pre-compilation of ``noarch`` packages, with no impact on capability to use the package but removing it will let artifacts (pycache) on the filesystem


Hangs during install in QEMU
----------------------------
When using Mamba/Micromamba inside a QEMU guest, installing packages may sometimes hang forever due to an `issue with QEMU and glibc <https://gitlab.com/qemu-project/qemu/-/issues/285>`_. As a workaround, set ``G_SLICE=always-malloc`` in the QEMU guest, eg.::

  export G_SLICE=always-malloc
  mamba install ...

See `#1611 <https://github.com/mamba-org/mamba/issues/1611>`_ for discussion.


Hangs during package installation on NFS (Network File Systems)
---------------------------------------------------------------
When using Mamba/Micromamba in a environment with NFS, package installation (e.g., NumPy) may hang at the step when ``libmamba`` attempts to lock a directory.
A solution is to update the Mamba/Micromamba configuration to not use lockfile by the command::

  micromamba config set use_lockfiles False

See `#2592 <https://github.com/mamba-org/mamba/issues/2592>`_, `#1446 <https://github.com/mamba-org/mamba/issues/1446>`_, `#1448 <https://github.com/mamba-org/mamba/pull/1448>`_, `#1515 <https://github.com/mamba-org/mamba/issues/1515>`_ for more details.


"libmamba Download error (7) Could not connect to server"
---------------------------------------------------------
``mamba install`` and other ``mamba`` commands yield said errors. This might be due to being flagged by an antivirus.
A solution is to whitelist the appropriate folders and files; see `#3979 <https://github.com/mamba-org/mamba/issues/3979>`_ for more details.
