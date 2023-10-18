.. _troubleshooting:

Troubleshooting
===============

Please use the official installer
---------------------------------

Please make sure that you use the :ref:`official Mambaforge installer <installation>` to install Mamba. Other installation methods are not supported.

Mamba should be installed to the ``base`` environment
-----------------------------------------------------

Installing Mamba to an environment other than ``base`` is not supported. Mamba must be installed in the ``base`` environment alongside Conda, and no other packages may be installed into ``base``.

No other packages should be installed to ``base``
-------------------------------------------------

Installing packages other than Conda and Mamba into the ``base`` environment is not supported. Mamba must live in the same environment as Conda, and Conda does not support having packages other than Conda itself and its dependencies in ``base``.

.. _defaults_channels:

Using the ``defaults`` channels
-------------------------------

It is **not recommended** to use the `Anaconda default channels <https://docs.anaconda.com/anaconda/user-guide/tasks/using-repositories/>`_:

- ``pkgs/main``
- ``pkgs/r`` / ``R``
- ``msys2``
- ``defaults`` (which includes all of the above)

Please note that we won't be able to help resolving any problems you might face with the Anaconda channels.

Please use ``conda-forge`` instead.

Please disable the default channels in your install command::

  mamba create --override-channels ...

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
    - nodefaults

Mixing the ``defaults`` and ``conda-forge`` channels
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The `Anaconda default channels <https://docs.anaconda.com/anaconda/user-guide/tasks/using-repositories/>`_ are **incompatible** with conda-forge.

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

- Reinitializing your shell with `micromamba shell reinit`.
- Deleting the package cache (`"package cache"` entries in `micromamba info`)

libmamba.so.2: undefined symbol ...
-----------------------------------

See :ref:`defaults_channels`.

Windows long paths
------------------

Windows API historically supports paths up to 260 characters. While it's now possible to used longer ones, there are still limitations related to that.

``libmamba`` internally relies on ``\\?\`` prefixing to handle such paths. If you get error messages advertising such prefix, please have look at the following steps:


Long paths support has to be activated
**************************************

source: Robocorp `troubleshooting documentation <https://robocorp.com/docs/troubleshooting/windows-long-path>`_

1. Open the Local Group Policy Editor application: - Start --> type gpedit.msc --> Enter:
2. Navigate to Computer Configuration > Administrative Templates > System > Filesystem. On the right, find the "Enable win32 long paths" item and double-click it
3. Change the setting to Enabled
4. Exit the Local Group Policy Editor and restart your computer (or sign out and back in) to allow the changes to finish

If the problem persists after those steps, try the following:

1. Open the Registry Editor application: - Start --> type regedit.msc and press Enter:
2. Navigate to HKEY-LOCAL-MACHINE > SYSTEM > CurrentControlSet > Control > FileSystem. On the right, find the LongPathsEnabled item and double-click it
3. Change the Value data: to 1
4. Exit the Registry Editor


cmd.exe does not support calls to long prefixes
***********************************************

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
