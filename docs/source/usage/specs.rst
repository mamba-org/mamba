Describing Conda Objects
========================

The :any:`libmambapy.specs <mamba::specs>` submodule contains object to *describe* abstraction in the Conda ecosystem.
They are purely functional and do not have any observable impact on the user system.
For instance :cpp:type:`libmambapy.specs.Channel <mamba::specs::Channel>` is used to describe a
channel but does not download any file.

CondaURL
--------
The :cpp:type:`CondaURL <mamba::specs::CondaURL>` is a rich URL object that has additional
capabilities for dealing with tokens, platforms, and packages.

To parse a string into a ``CondaURL``, use ``CondaURL.parse`` as follows:

.. code:: python

   import libmambapy.specs as specs

   url = specs.CondaURL.parse(
       "https://conda.anaconda.org/t/someprivatetoken/conda-forge/linux-64/x264-1%21164.3095-h166bdaf_2.tar.bz2"
   )
   assert url.host() == "conda.anaconda.org"
   assert url.package() == "x264-1!164.3095-h166bdaf_2.tar.bz2"
   assert url.package(decode=False) == "x264-1%21164.3095-h166bdaf_2.tar.bz2"

The :cpp:func:`CondaURL.parse <mamba::specs::CondaURL::parse>` method assumes that the URL is
properly `percent encoded <https://en.wikipedia.org/wiki/Percent-encoding>`_.
For instance, here the character ``!`` in the file name ``x264-1!164.3095-h166bdaf_2.tar.bz2`` had
to be replaced with ``%21``.
The getter functions, such as :cpp:func:`CondaURL.package <mamba::specs::CondaURL::package>`
automatically decoded it for us, but we can specify ``decode=False`` to keep the raw representation.
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

Path manipulation is handled automatically, either with
:cpp:func:`CondaURL.append_path <mamba::specs::CondaURL::append_path>` or the ``/`` operator.

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

The function :cpp:func:`CondaURL.str <mamba::specs::CondaURL::str>` can be used to get a raw
representation of the string. By default, it will hide all credentials

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

Similarily the :cpp:func:`CondaURL.pretty_str <mamba::specs::CondaURL::pretty_str>` returns a more
user-friendly string, but that may not be parsed back.


UnresolvedChannel
-----------------

A :cpp:type:`UnresolvedChannel <mamba::specs::UnresolvedChannel>` is a lightweight object to represent
a channel string, as in passed in the CLI or configuration.
Since channels rely heavily on configuration options, this type can be used as a placeholder for a
channel that has not been fully "resolved" to a specific location.
It does minimal parsing and can detect the type of ressource (an unresolved name, a URL, a file)
and the platform filters.

.. code:: python

   import libmambapy.specs as specs

   uc = specs.UnresolvedChannel.parse("https://conda.anaconda.org/conda-forge/linux-64")

   assert uc.location == "https://conda.anaconda.org/conda-forge"
   assert uc.platform_filters == {"linux-64"}
   assert uc.type == specs.UnresolvedChannel.Type.URL

Dynamic platforms (as in not known by Mamba) can only be detected with the ``[]`` syntax.

.. code:: python

   import libmambapy.specs as specs

   uc = specs.UnresolvedChannel.parse("conda-forge[prius-avx42]")

   assert uc.location == "conda-forge"
   assert uc.platform_filters == {"prius-avx42"}
   assert uc.type == specs.UnresolvedChannel.Type.Name


.. _libmamba_usage_channel:
Channel
-------
The :cpp:type:`Channel <mamba::specs::Channel>` are represented by a
:cpp:type:`CondaURL <mamba::specs::CondaURL>` and a set of platform filters.
A display name is also available, but is not considered a stable identifiaction form of the
channel, since it depends on the many configuration parameters, such as the channel alias.

We construct a :cpp:type:`Channel <mamba::specs::Channel>` by *resolving* a
:cpp:type:`UnresolvedChannel <mamba::specs::UnresolvedChannel>`.
All parameters that influence this resolution must be provided explicitly.


.. code:: python

   import libmambapy.specs as specs

   uc = specs.UnresolvedChannel.parse("conda-forge[prius-avx42]")
   chan, *_ = specs.Channel.resolve(
       uc,
       channel_alias="https://repo.mamba.pm"
       # ...
   )

   assert chan.url.str() == "https://repo.mamba.pm/conda-forge"
   assert chan.platforms == {"prius-avx42"}
   assert chan.display_name == "conda-forge[prius-avx42]"

There are no hard-coded names:

.. code:: python

   import libmambapy.specs as specs

   uc = specs.UnresolvedChannel.parse("defaults")
   chan, *_ = specs.Channel.resolve(
       uc,
       channel_alias="https://repo.mamba.pm"
       # ...
   )

   assert chan.url.str() == "https://repo.mamba.pm/defaults"

You may have noticed that :cpp:func:`Channel.resolve <mamba::specs::Channel::resolve>` returns
multiple channels.
This is because of custom multichannel, a single name can return mutliple channels.


.. code:: python

   import libmambapy.specs as specs

   chan_main, *_ = specs.Channel.resolve(
       specs.UnresolvedChannel.parse("pkgs/main"),
       # ...
   )
   chan_r, *_ = specs.Channel.resolve(
       specs.UnresolvedChannel.parse("pkgs/r"),
       # ...
   )

   defaults = specs.Channel.resolve(
       specs.UnresolvedChannel.parse("defaults"),
       custom_multichannels=specs.Channel.MultiChannelMap(
           {"defaults": [chan_main, chan_r]}
       ),
       # ...
   )

   assert defaults == [chan_main, chan_r]

.. note::

   Creating :cpp:type:`Channel <mamba::specs::Channel>` objects this way, while highly
   customizable, can be very verbose.
   In practice, one can create a ``ChannelContext`` with ``ChannelContext.make_simple`` or
   ``ChannelContext.make_conda_compatible`` to compute and hold all these parameters from a
   ``Context`` (itself getting its values from all the configuration sources).
   ``ChannelContext.make_channel`` can then directly construct a
   :cpp:type:`Channel <mamba::specs::Channel>` from a string.


Version
-------
In the conda ecosystem, a version is an epoch and a pair of arbitrary length sequences of arbitrary
length sequences of string and integer pairs.
Let's unpack this with an example.
The version ``1.2.3`` is the outer sequence, it can actually contain as many elements as needed
so ``1.2.3.4.5.6.7`` is also a valid version.
For alpha version, we sometimes see something like ``1.0.0alpha1``.
That's the inner sequence of pairs, for the last part ``[(0, "alpha"), (1, "")]``.
There can also be any number, such as in ``1.0.0alpha1dev3``.
We can specify another *"local"* version, that we can separate with a ``+``, as in ``1.9.0+2.0.0``,
but that is not widely used.
Finally, there is also an epoch, similar to `PEP440 <https://peps.python.org/pep-0440/>`_, to
accomodate for change in the versioning scheme.
For instance, in ``1!2.0.3``, the epoch is ``1``.

To sum up, a version like ``7!1.2a3.5b4dev+1.3.0``, can be parsed as:

- **epoch**: ``7``,
- **version**: ``[[(1, "")], [(2, "a"), (3, "")], [(5, "b"), (4, "dev")]]``
- **local version**: ``[[(1, "")], [(3, "")], [(0, "")]]``

Finally, all versions are considered equal to the same version with any number of trailing zeros,
so ``1.2``, ``1.2.0``, and ``1.2.0.0`` are all considered equal.

.. warning::

   The flexibility of conda versions (arguably too flexible) is meant to accomodate differences
   in various ecosystems.
   Library authors should stick to well defined version schemes such as
   `semantic versioning <https://semver.org/>`_,
   `calendar versioning <https://calver.org/>`_, or
   `PEP440 <https://peps.python.org/pep-0440/>`_.

A :cpp:type:`Version <mamba::specs::Version>` can be created by parsing a string with
:cpp:func:`Version.parse <mamba::specs::Version::parse>`.

.. code:: python

   import libmambapy.specs as specs

   v = specs.Version.parse("7!1.2a3.5b4dev+1.3.0")


The most useful operations on versions is to compare them.
All comparison operators are available:

.. code:: python

   import libmambapy.specs as specs

   assert specs.Version.parse("1.2.0") == specs.Version.parse("1.2.0.0")
   assert specs.Version.parse("1.2.0") < specs.Version.parse("1.3")
   assert specs.Version.parse("2!4.0.0") >= specs.Version.parse("1.8")


VersionSpec
-----------
A version spec is a way to describe a set of versions.
We have the following primitives:

- ``*`` matches all versions (unrestricted).
- ``==`` for **equal** states matches versions equal to the given one (a singleton).
  For instance ``==1.2.4`` matches ``1.2.4`` only, and not ``1.2.4.1`` or ``1.2``.
  Note that since ``1.2.4.0`` is the same as ``1.2.4``, this is also matched.
- ``!=`` for ``not equal`` is the opposite, it matches all but the given version.
  For instance ``=!1.2.4`` matches ``1.2.5`` and ``1!1.2.4`` but not ``1.2.4``.
- ``>`` for **greater** matches versions stricly greater than the current one, for instance
  ``>1.2.4`` matches ``2.0.0``, ``1!1.0.0``, but not ``1.1.0`` or ``1.2.4``.
- ``>=`` for **greater or equal**.
- ``<`` for **less**.
- ``<-`` for **less or equal**.
- ``=`` for **starts with** matches versions that start with the same non zero parts of the version.
  For instance ``=1.7`` matches ``1.7.8``, and ``1.7.0alpha1`` (beware since this is smaller
  than ``1.7.0``).
  This spec can equivalently be written ``1.7`` (bare), ``1.7.*``, or ``=1.7.*``.
- ``=!``  with ``.*`` for **not starts with** matches all versions but the one that starts with
  the non zero parts specified.
  For instance ``!=1.7.*`` matches ``1.8.3`` but not ``1.7.2``.
- ``~=`` for **compatible with** matches versions that are greater or equal and starting with the
  all but the last parts specified, including zeros.
  For instance `~=2.0` matches ``2.0.0``, ``2.1.3``, but not ``3.0.1`` or ``2.0.0alpha``.

All version spec can be combine using a boolean grammar where ``|`` means **or** and ``,`` means
**and**.
For instance, ``(>2.1.0,<3.0)|==2.0.1`` means:

- Either
   - equal to ``2.0.1``,
   - or, both
     - greater that ``2.1.0``
     - and less than ``3.0``.

To create a :cpp:type:`VersionSpec <mamba::specs::VersionSpec>` from a string, we parse it with
:cpp:type:`VersionSpec.parse <mamba::specs::VersionSpec::parse>`.
To check if a given version matches a version spec, we use
:cpp:type:`VersionSpec.contains <mamba::specs::VersionSpec::contains>`.

.. code:: python

   import libmambapy.specs as specs

   vs = specs.VersionSpec.parse("(>2.1.0,<3.0)|==2.0.1")

   assert vs.contains(specs.Version.parse("2.4.0"))
   assert vs.contains(specs.Version.parse("2.0.1"))
   assert not vs.contains(specs.Version.parse("3.0.1"))
