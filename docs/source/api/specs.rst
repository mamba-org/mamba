Describing Conda Objects
========================

The ``libmambapy.specs`` submodule contains object to *describe* abstraction in the Conda ecosystem.
They are purely functional and do not have any observable impact on the user system.
For instance ``libmambapy.specs.Channel`` is used to describe a channel but does not download any file.

CondaURL
--------
The ``CondaURL`` is a rich URL object that has additional capabilities for dealing with tokens,
platforms, and packages.

To parse a string into a ``CondaURL``, use ``CondaURL.parse`` as follows:

.. code:: python

   import libmambapy.specs as specs

   url = specs.CondaURL.parse(
       "https://conda.anaconda.org/t/someprivatetoken/conda-forge/linux-64/x264-1%21164.3095-h166bdaf_2.tar.bz2"
   )
   assert url.host() == "conda.anaconda.org"
   assert url.package() == "x264-1!164.3095-h166bdaf_2.tar.bz2"
   assert url.package(decode=False) == "x264-1%21164.3095-h166bdaf_2.tar.bz2"

The ``CondaURL.parse`` method assumes that the URL is properly
`percent encoded <https://en.wikipedia.org/wiki/Percent-encoding>`_.
For instance, here the character ``!`` in the file name ``x264-1!164.3095-h166bdaf_2.tar.bz2`` had
to be replaced with ``%21``.
The getter functions, such as ``CondaURL.package`` automatically decoded it for us, but we can
specify ``decode=False`` to keep the raw representation.
The setters follow the same logic, as described bellow.

.. code:: python

   import libmambapy.specs as specs

   url = specs.CondaURL()
   url.set_host("mamba.pm")
   url.set_user("my%20name", encode=False)
   url.set_password("n&#4d!3gfsd")

   assert url.user() == "my name"
   assert url.user(decode=False) == "my%20name"
   assert url.password() == "n&#4d!3gfsd"
   assert url.password(decode=False) == "n%26%234d%213gfsd"

Path manipulation is handled automatically, either with ``CondaURL.append_path`` or the ``/``
operator.

.. code:: python

   import libmambapy.specs as specs

   url1 = specs.CondaURL.parse("mamba.pm")
   url2 = url / "/t/xy-12345678-1234/conda-forge/linux-64"

   assert url1.path() == "/"
   assert url2.path() == "/t/xy-12345678-1234/conda-forge/linux-64"
   assert url2.path_without_token() == "/conda-forge/linux-64"

You can always assume that the paths returned will start with a leading ``/``.
As always, encoding and decoding options are available.

.. note::

   Contrary to ``pathlib.Path``, the ``/`` operator will always append, even when the sub-path
   starts with a ``/``.

The function ``CondaURL.str`` can be used to get a raw representation of the string.
By default, it will hide all credentials

.. code:: python

   import libmambapy.specs as specs

   url = specs.CondaURL.parse("mamba.pm/conda-forge")
   url.set_user("user@mail.com")
   url.set_password("private")
   url.set_token("xy-12345678-1234")

   assert url.str() == "https://user%40mail.com:*****@mamba.pm/t/*****"
   assert (
       url.str(credentials="Show")
       == "https://user%40mail.com:private@mamba.pm/t/xy-12345678-1234"
   )
   assert url.str(credentials="Remove") == "https://mamba.pm/"

Similarily the ``CondaURL.pretty_str`` returns a more user-friendly string, but that may not be
parsed back.
