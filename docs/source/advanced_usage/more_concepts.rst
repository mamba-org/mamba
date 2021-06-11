.. _more_concepts:

More concepts
=============

Overview
--------

| This section complements :ref:`concepts<concepts>` with advanced terminology.

While not necessary to understand the basic usage, those ``advanced concepts`` are fundamental to understand Mamba in details.


.. _linking:

Linking
-------

Each package contains its files index, used at a step called ``linking``.
The ``linking`` consists in creating a *link* between the package cache where the tarballs are expanded into multiple directories/files and the installation :ref:`target prefix<prefix>`.

The 3 kinds of *links* are:

- :ref:`hard-link<hard_link>`
- :ref:`soft-link<soft_link>`
- :ref:`copy<copy>`


| The default behavior is to use ``hard-links`` and fallback to ``copy``.

The advanced user may want to change that behavior using configuration (see the relevant CLI or API reference for more details):

- allow ``soft-links`` to be used as a prefered fallback to ``copy`` (try to ``copy`` if ``soft-link`` fails)
- use ``soft-links`` instead of ``hard-links`` as default behavior (``copy`` is still a fallback)
- always ``copy`` instead of ``hard-links`` as default behavior (no fallback then)

.. warning::
   ``soft-links`` more easily lead to broken environment due to their property of becoming invalid when the target is deleted/moved


.. _hard_link:

Hard-link
*********

| A ``hard-link`` is the relation between a name/path and the actual file located on the file system.
| It is often used to describe additional ``hard-links`` pointed the same file, but the ownership of the file is shared accross all those links (equivalent to a C++ shared pointer):

- a reference counter is incremented when creating a new ``hard-link``, decremented when deleting one
- the file system location is freed only when that counter decreases to 0

.. image:: hard_links.png
  :height: 300
  :align: center

source: `Wikipedia <https://en.wikipedia.org/wiki/Hard_link>`_


This is the most efficient way to link:

- the underlying file on the file system is not duplicated

  - it is super efficient and resource friendly

- ``hard-link`` stays valid when another ``hard-link`` to the same reference is deleted/moved

There are some limitations to use ``hard-links``:

- all the file systems are not supporting such links
- those links are not working accross file systems/partitions


.. _soft_link:

Soft-link
*********

| A ``soft-link``, also called ``symlink`` (symbolic link), is much more similar to a shortcut or a redirection to another name.

It is as efficient as a ``hard-link`` but has different properties:

- works accross a filesystem/partition boundaries
- becomes invalid then the pointed name is deleted or moved (no shared ownership)


.. _copy:

Copy
****

| This is a most well-known link, a simple copy of the file is done.

It is not efficient nor resource friendly but preserve the file from deletion/modification of the reference.
