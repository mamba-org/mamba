.. _troubleshooting:

Troubleshooting
===============

Mamba should be installed to the ``base`` environment
-----------------------------------------------------

Installing Mamba to an environment other than ``base`` is not supported. Mamba must be installed in the ``base`` environment alongside Conda, and no other packages may be installed into ``base``.

No other packages should be installed to ``base``
-------------------------------------------------

Installing packages other than Conda and Mamba into the ``base`` environment is not supported. Mamba must live in the same environment as Conda, and Conda does not support having packages other than Conda itself and its dependencies in ``base``.

Mixing the ``defaults`` channels and ``conda-forge``
----------------------------------------------------

Using the ``defaults`` and ``conda-forge`` channels at the same time is not supported, eg. using a channel configuration like this:

.. code-block:: yaml

  channels:
    - conda-forge
    - defaults

The `Anaconda default channels <https://docs.anaconda.com/anaconda/user-guide/tasks/using-repositories/>`_ are incompatible with conda-forge.

Mamba broken after Conda update
-------------------------------

Mamba sometimes stops working if you update to a very recent version of Conda. Please downgrade to the latest working a version and file a bug report in the Mamba bug tracker if the problem has not been reported yet.

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
