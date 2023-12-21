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


ChannelSpec
-----------
A ``ChannelSpec`` is a lightweight object to represent a channel string, as in passed in the CLI
or configuration.
It does minimal parsing and can detect the type of ressource (an unresolved name, a URL, a file)
and the platform filters.

.. code:: python

   import libmambapy.specs as specs

   cs = specs.ChannelSpec.parse("https://conda.anaconda.org/conda-forge/linux-64")

   assert cs.location == "https://conda.anaconda.org/conda-forge"
   assert cs.platform_filters == {"linux-64"}
   assert cs.type == specs.ChannelSpec.Type.URL

Dynamic platforms (as in not known by Mamba) can only be detected with the ``[]`` syntax.

.. code:: python

   import libmambapy.specs as specs

   cs = specs.ChannelSpec.parse("conda-forge[prius-avx42]")

   assert cs.location == "conda-forge"
   assert cs.platform_filters == {"prius-avx42"}
   assert cs.type == specs.ChannelSpec.Type.Name


Channel
-------
The ``Channel`` are represented by a ``CondaURL`` and a set of platform filters.
A display name is also available, but is not considered a stable identifiaction form of the
channel, since it depends on the many configuration parameters, such as the channel alias.

We construct a ``Channel`` by *resolving* a ``ChannelSpec``.
All parameters that influence this resolution must be provided explicitly

.. code:: python

   import libmambapy.specs as specs

   cs = specs.ChannelSpec.parse("conda-forge[prius-avx42]")
   chan, *_ = specs.Channel.resolve(
       spec=cs,
       channel_alias="https://repo.mamba.pm"
       # ...
   )

   assert chan.url.str() == "https://repo.mamba.pm/conda-forge"
   assert cs.platforms == {"prius-avx42"}
   assert chan.display_name == "conda-forge[prius-avx42]"

There are no hard-coded names:

.. code:: python

   import libmambapy.specs as specs

   cs = specs.ChannelSpec.parse("defaults")
   chan, *_ = specs.Channel.resolve(
       spec=cs,
       channel_alias="https://repo.mamba.pm"
       # ...
   )

   assert chan.url.str() == "https://repo.mamba.pm/defaults"

You may have noticed that ``Channel.resolve`` returns multiple channels.
This is because of custom multichannel, a single name can return mutliple channels.


.. code:: python

   import libmambapy.specs as specs

   chan_main, *_ = specs.Channel.resolve(
       spec=specs.ChannelSpec.parse("pkgs/main"),
       # ...
   )
   chan_r, *_ = specs.Channel.resolve(
       spec=specs.ChannelSpec.parse("pkgs/r"),
       # ...
   )

   defaults = specs.Channel.resolve(
       spec=specs.ChannelSpec.parse("defaults"),
       custom_multichannels=specs.Channel.MultiChannelMap(
           {"defaults": [chan_main, chan_r]}
       ),
       # ...
   )

   assert defaults == [chan_main, chan_r]

.. note::

   Creating ``Channel`` objects this way, while highly customizable, can be very verbose.
   In practice, one can create a ``ChannelContext`` with ``ChannelContext.make_simple`` or
   ``ChannelContext.make_conda_compatible`` to compute and hold all these parameters from a
   ``Context`` (itself getting its values from all the configuration sources).
   ``ChannelContext.make_channel`` can then directly construct a ``Channel`` from a string.
