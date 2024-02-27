import copy

import pytest

import libmambapy


def test_import_submodule():
    import libmambapy.specs as specs

    # Dummy execution
    _p = specs.Platform.noarch


def test_import_recursive():
    import libmambapy as mamba

    # Dummy execution
    _p = mamba.specs.Platform.noarch


def test_archive_extension():
    assert libmambapy.specs.archive_extensions() == [".tar.bz2", ".conda", ".whl"]

    assert libmambapy.specs.has_archive_extension("pkg.conda")
    assert not libmambapy.specs.has_archive_extension("conda.pkg")

    assert libmambapy.specs.strip_archive_extension("pkg.conda") == "pkg"
    assert libmambapy.specs.strip_archive_extension("conda.pkg") == "conda.pkg"


def test_Platform():
    Platform = libmambapy.specs.Platform

    assert Platform.noarch.name == "noarch"
    assert Platform.linux_32.name == "linux_32"
    assert Platform.linux_64.name == "linux_64"
    assert Platform.linux_armv6l.name == "linux_armv6l"
    assert Platform.linux_armv7l.name == "linux_armv7l"
    assert Platform.linux_aarch64.name == "linux_aarch64"
    assert Platform.linux_ppc64le.name == "linux_ppc64le"
    assert Platform.linux_ppc64.name == "linux_ppc64"
    assert Platform.linux_s390x.name == "linux_s390x"
    assert Platform.linux_riscv32.name == "linux_riscv32"
    assert Platform.linux_riscv64.name == "linux_riscv64"
    assert Platform.osx_64.name == "osx_64"
    assert Platform.osx_arm64.name == "osx_arm64"
    assert Platform.win_32.name == "win_32"
    assert Platform.win_64.name == "win_64"
    assert Platform.win_arm64.name == "win_arm64"
    assert Platform.zos_z.name == "zos_z"

    assert len(Platform.__members__) == Platform.count()
    assert Platform.build_platform() in Platform.__members__.values()
    assert Platform.parse("linux-64") == Platform.linux_64
    assert Platform("linux_64") == Platform.linux_64

    with pytest.raises(KeyError):
        # No parsing, explicit name
        Platform("linux-64")


def test_NoArchType():
    NoArchType = libmambapy.specs.NoArchType

    assert NoArchType.No.name == "No"
    assert NoArchType.Generic.name == "Generic"
    assert NoArchType.Python.name == "Python"

    assert len(NoArchType.__members__) == NoArchType.count()
    assert NoArchType.parse(" Python") == NoArchType.Python
    assert NoArchType("Generic") == NoArchType.Generic

    with pytest.raises(KeyError):
        # No parsing, explicit name, needs "Generic"
        NoArchType("generic")


def test_CondaURL_Credentials():
    Credentials = libmambapy.specs.CondaURL.Credentials

    assert Credentials.Hide.name == "Hide"
    assert Credentials.Show.name == "Show"
    assert Credentials.Remove.name == "Remove"
    assert Credentials("Show") == Credentials.Show


def test_CondaURL_setters():
    CondaURL = libmambapy.specs.CondaURL
    url = CondaURL()

    # Scheme
    assert url.scheme_is_defaulted()
    url.set_scheme("ftp")
    assert not url.scheme_is_defaulted()
    assert url.scheme() == "ftp"
    assert url.clear_scheme() == "ftp"
    assert url.scheme() == "https"
    assert url.scheme_is_defaulted()
    # User
    assert url.user() == ""
    assert not url.has_user()
    url.set_user("user@email.com")
    assert url.user() == "user@email.com"
    url.set_user("none%40email.com", encode=False)
    assert url.user() == "none@email.com"
    assert url.clear_user() == "none%40email.com"
    assert url.user() == ""
    # Password
    assert url.password() == ""
    assert not url.has_password()
    url.set_password("some#pass")
    assert url.password() == "some#pass"
    url.set_password("s%23pass", encode=False)
    assert url.password() == "s#pass"
    assert url.clear_password() == "s%23pass"
    assert url.password() == ""
    # Host
    assert url.host_is_defaulted()
    assert url.host() == "localhost"
    url.set_host("mamba.org")
    assert not url.host_is_defaulted()
    assert url.host() == "mamba.org"
    assert url.clear_host() == "mamba.org"
    assert url.host() == "localhost"
    assert url.host_is_defaulted()
    # Port
    assert url.port() == ""
    url.set_port("33")
    assert url.port() == "33"
    assert url.clear_port() == "33"
    assert url.port() == ""
    # Path
    assert url.path() == "/"
    url.set_path("some path")
    assert url.path() == "/some path"
    url.set_path("some%20", encode=False)
    assert url.path() == "/some "
    url.append_path(" foo")
    assert url.path() == "/some / foo"
    url.append_path("%23", encode=False)
    assert url.path() == "/some / foo/#"
    assert url.clear_path() == "/some%20/%20foo/%23"
    # Token
    assert not url.has_token()
    assert url.token() == ""
    url.set_token("mytoken")
    assert url.has_token()
    assert url.token() == "mytoken"
    assert url.clear_token()
    assert url.token() == ""
    # Path without token
    url.set_token("mytoken")
    assert url.path_without_token() == "/"
    url.set_path_without_token("my path")
    assert url.path_without_token() == "/my path"
    url.set_path_without_token("bar%20", encode=False)
    assert url.path_without_token() == "/bar "
    assert url.clear_path_without_token()
    # Platform
    url.set_path_without_token("conda-forge/win-64", encode=False)
    assert url.platform().name == "win_64"
    url.set_platform("linux_64")
    assert url.platform().name == "linux_64"
    assert url.clear_platform()
    # Package
    assert url.package() == ""
    url.set_package("pkg.conda")
    assert url.package() == "pkg.conda"
    assert url.clear_package()


def test_CondaURL_parse():
    CondaURL = libmambapy.specs.CondaURL

    url = CondaURL.parse(
        "https://user%40mail.com:pas%23@repo.mamba.pm:400/t/xy-12345678-1234/%20conda/linux-64/pkg.conda"
    )
    assert url.scheme() == "https"
    assert url.user() == "user@mail.com"
    assert url.user(decode=False) == "user%40mail.com"
    assert url.password() == "pas#"
    assert url.password(decode=False) == "pas%23"
    assert url.authentication() == "user%40mail.com:pas%23"
    assert url.host() == "repo.mamba.pm"
    assert url.host(decode=False) == "repo.mamba.pm"
    assert url.port() == "400"
    assert url.path() == "/t/xy-12345678-1234/ conda/linux-64/pkg.conda"
    assert url.path(decode=False) == "/t/xy-12345678-1234/%20conda/linux-64/pkg.conda"
    assert url.token() == "xy-12345678-1234"
    assert url.path_without_token() == "/ conda/linux-64/pkg.conda"
    assert url.path_without_token(decode=False) == "/%20conda/linux-64/pkg.conda"
    assert url.platform().name == "linux_64"
    assert url.package() == "pkg.conda"

    assert (
        url.str()
        == "https://user%40mail.com:*****@repo.mamba.pm:400/t/*****/%20conda/linux-64/pkg.conda"
    )
    assert (
        url.str(credentials="Show")
        == "https://user%40mail.com:pas%23@repo.mamba.pm:400/t/xy-12345678-1234/%20conda/linux-64/pkg.conda"
    )
    assert repr(url) == (
        "https://user%40mail.com:*****@repo.mamba.pm:400/t/*****/%20conda/linux-64/pkg.conda"
    )
    assert (
        url.pretty_str(strip_scheme=True, credentials="Hide", rstrip_path="/")
        == "user@mail.com:*****@repo.mamba.pm:400/t/*****/ conda/linux-64/pkg.conda"
    )


def test_CondaURL_op():
    CondaURL = libmambapy.specs.CondaURL
    url = CondaURL.parse(
        "https://user%40mail.com:pas%23@repo.mamba.pm:400/t/xy-12345678-1234/%20conda/linux-64/pkg.conda"
    )

    #  Copy
    other = copy.deepcopy(url)
    assert other.str() == url.str()
    assert other is not url

    # Comparison
    assert hash(url) != 0
    assert url == url
    assert other == url
    other.set_host("somehost.com")
    assert other != url

    # Append
    url.set_path("/folder")
    assert (url / "file.txt").path() == "/folder/file.txt"


def test_UnresolvedChannel_Type():
    Type = libmambapy.specs.UnresolvedChannel.Type

    assert Type.URL.name == "URL"
    assert Type.PackageURL.name == "PackageURL"
    assert Type.Path.name == "Path"
    assert Type.PackagePath.name == "PackagePath"
    assert Type.Name.name == "Name"
    assert Type.Unknown.name == "Unknown"
    assert Type("Name").name == "Name"


def test_UnresolvedChannel():
    UnresolvedChannel = libmambapy.specs.UnresolvedChannel

    # Constructor
    uc = UnresolvedChannel(
        location="<unknown>",
        platform_filters=set(),
        type=UnresolvedChannel.Type.Unknown,
    )
    assert uc.location == "<unknown>"
    assert uc.platform_filters == set()
    assert uc.type == UnresolvedChannel.Type.Unknown

    # Enum cast
    uc = UnresolvedChannel(location="conda-forge", platform_filters=set(), type="Name")
    assert uc.type == UnresolvedChannel.Type.Name

    #  Parser
    uc = UnresolvedChannel.parse("conda-forge[linux-64]")
    assert uc.location == "conda-forge"
    assert uc.platform_filters == {"linux-64"}
    assert uc.type == UnresolvedChannel.Type.Name

    #  Copy
    other = copy.deepcopy(uc)
    assert other.location == uc.location
    assert other is not uc


def test_BasicHTTPAuthentication():
    BasicHTTPAuthentication = libmambapy.specs.BasicHTTPAuthentication

    auth = BasicHTTPAuthentication(user="mamba", password="superpass!!")

    # Properties
    assert auth.user == "mamba"
    assert auth.password == "superpass!!"
    auth.user = "conda"
    auth.password = "awesome#"
    assert auth.user == "conda"
    assert auth.password == "awesome#"

    #  Copy
    other = copy.deepcopy(auth)
    assert other is not auth
    assert other.user == auth.user
    assert other is not auth

    # Comparion
    assert auth == auth
    assert auth == other
    other.user = "rattler"
    assert auth != other
    assert hash(auth) != 0


def test_BearerToken():
    BearerToken = libmambapy.specs.BearerToken

    auth = BearerToken(token="mytoken")

    # Properties
    assert auth.token == "mytoken"
    auth.token = "othertok"
    assert auth.token == "othertok"

    #  Copy
    other = copy.deepcopy(auth)
    assert other is not auth
    assert other.token == auth.token

    # Comparion
    assert auth == auth
    assert auth == other
    other.token = "foo"
    assert auth != other
    assert hash(auth) != 0


def test_CondaToken():
    CondaToken = libmambapy.specs.CondaToken

    auth = CondaToken(token="mytoken")

    # Properties
    assert auth.token == "mytoken"
    auth.token = "othertok"
    assert auth.token == "othertok"

    #  Copy
    other = copy.deepcopy(auth)
    assert other is not auth
    assert other.token == auth.token

    # Comparison
    assert auth == auth
    assert auth == other
    other.token = "foo"
    assert auth != other
    assert hash(auth) != 0


def test_AuthenticationDataBase():
    AuthenticationDataBase = libmambapy.specs.AuthenticationDataBase
    BearerToken = libmambapy.specs.BearerToken
    BasicHTTPAuthentication = libmambapy.specs.BasicHTTPAuthentication

    auth_1 = BearerToken(token="mytoken")
    db = AuthenticationDataBase({"mamba.org": auth_1})

    # Dict style mapping
    assert "mamba.org" in db
    assert len(db) == 1
    assert db["mamba.org"] == auth_1

    auth_2 = BasicHTTPAuthentication(user="user", password="pass")
    db["anaconda.com"] = auth_2
    assert db["anaconda.com"] == auth_2

    #  Iteration
    for key, val in db.items():
        ...

    # Comparison
    assert auth_1 == auth_1
    assert auth_1 != auth_2

    # Special functions
    assert db.contains_weaken("mamba.org")
    assert db.contains_weaken("mamba.org/conda-forge")
    assert db.at_weaken("mamba.org/conda-forge") == auth_1


def test_ChannelResolveParams():
    ChannelResolveParams = libmambapy.specs.ChannelResolveParams
    CondaURL = libmambapy.specs.CondaURL
    AuthenticationDataBase = libmambapy.specs.AuthenticationDataBase
    BearerToken = libmambapy.specs.BearerToken

    # custom_channel and custom_multichannel require creating a Channel to be tested,
    # so we leave them out for simplicity/isolation here.
    # See test_Channel_resolve

    platforms_1 = {"linux-64"}
    channel_alias_1 = CondaURL.parse("oci://github.com/")
    db_1 = AuthenticationDataBase({"mamba.org": BearerToken(token="mytoken")})
    home_dir_1 = "/users/mamba"
    cwd_1 = "/tmp/workspace"

    # Constructor
    params = ChannelResolveParams(
        platforms=platforms_1,
        channel_alias=channel_alias_1,
        #  custom_channels = ,
        #  custom_multichannels = ,
        authentication_db=db_1,
        home_dir=home_dir_1,
        current_working_dir=cwd_1,
    )

    # Getters
    assert params.platforms == platforms_1
    assert params.channel_alias == channel_alias_1
    assert params.authentication_db == db_1
    assert params.home_dir == home_dir_1
    assert params.current_working_dir == cwd_1

    platforms_2 = {"win-64", "noarch"}
    channel_alias_2 = CondaURL.parse("ftp://anaconda.com/")
    db_2 = AuthenticationDataBase({"conda.org": BearerToken(token="tkn")})
    home_dir_2 = "/users/conda"
    cwd_2 = "/tmp/elsewhere"

    # Setters
    params.platforms = platforms_2
    params.channel_alias = channel_alias_2
    params.authentication_db = db_2
    params.home_dir = home_dir_2
    params.current_working_dir = cwd_2
    assert params.platforms == platforms_2
    assert params.channel_alias == channel_alias_2
    assert params.authentication_db == db_2
    assert params.home_dir == home_dir_2
    assert params.current_working_dir == cwd_2


def test_Channel():
    Channel = libmambapy.specs.Channel
    Match = libmambapy.specs.Channel.Match
    CondaURL = libmambapy.specs.CondaURL

    url_1 = CondaURL.parse("https://repo.anaconda.com/conda-forge")
    platforms_1 = {"osx-64", "noarch"}
    display_name_1 = "conda-forge"

    # Constructor
    chan = Channel(url=url_1, platforms=platforms_1, display_name=display_name_1)

    # Gettters
    assert chan.url == url_1
    assert chan.platforms == platforms_1
    assert chan.display_name == display_name_1
    assert not chan.is_package()

    url_2 = CondaURL.parse("https://mamba.pm/mamba-forge/pkg.conda")
    platforms_2 = {"win-64"}
    display_name_2 = "mamba-forge"

    # Setters
    chan.url = url_2
    chan.platforms = platforms_2
    chan.display_name = display_name_2
    assert chan.url == url_2
    assert chan.platforms == platforms_2
    assert chan.display_name == display_name_2
    assert chan.is_package()

    #  Copy
    other = copy.deepcopy(chan)
    assert other is not chan
    assert chan.display_name == chan.display_name

    # Comparison
    assert chan == chan
    assert chan == chan
    other.platforms = {"neverheard-59"}
    assert chan != other
    assert hash(chan) != 0

    # Weak comparison
    chan = Channel(url=url_1, platforms=platforms_1, display_name=display_name_1)
    other = Channel(
        url=url_1, platforms=(chan.platforms | {"human-67"}), display_name=display_name_1
    )
    assert chan.url_equivalent_with(chan)
    assert chan.url_equivalent_with(other)
    assert other.url_equivalent_with(chan)
    assert chan.is_equivalent_to(chan)
    assert not chan.is_equivalent_to(other)
    assert not other.is_equivalent_to(chan)
    assert chan.contains_equivalent(chan)
    assert other.contains_equivalent(chan)
    assert not chan.contains_equivalent(other)
    assert chan.contains_package(chan.url / "noarch/pkg.conda") == Match.Full
    assert chan.contains_package(chan.url / "win-64/pkg.conda") == Match.InOtherPlatform


def test_Channel_resolve():
    Channel = libmambapy.specs.Channel
    UnresolvedChannel = libmambapy.specs.UnresolvedChannel
    CondaURL = libmambapy.specs.CondaURL
    AuthenticationDataBase = libmambapy.specs.AuthenticationDataBase
    BearerToken = libmambapy.specs.BearerToken

    platforms = {"linux-64"}
    channel_alias = CondaURL.parse("oci://github.com/")
    auth_db = AuthenticationDataBase({"mamba.org": BearerToken(token="mytoken")})
    home_dir = "/users/mamba"
    cwd = "/tmp/workspace"

    chans = Channel.resolve(
        what=UnresolvedChannel.parse("conda-forge"),
        platforms=platforms,
        channel_alias=channel_alias,
        authentication_db=auth_db,
        home_dir=home_dir,
        current_working_dir=cwd,
    )

    assert len(chans) == 1
    chan_1 = chans[0]
    assert chan_1.url == channel_alias / "conda-forge"
    assert chan_1.platforms == platforms
    assert chan_1.display_name == "conda-forge"

    # Custom channel match
    custom_channels = Channel.ChannelMap({"best-forge": chan_1})
    chans = Channel.resolve(
        what=UnresolvedChannel.parse("best-forge"),
        platforms=platforms,
        channel_alias=channel_alias,
        custom_channels=custom_channels,
        authentication_db=auth_db,
        home_dir=home_dir,
        current_working_dir=cwd,
    )
    assert len(chans) == 1
    chan_2 = chans[0]
    assert chan_2.display_name == "best-forge"

    # Custom multi channel match
    custom_multichannels = Channel.MultiChannelMap({"known-forges": [chan_1, chan_2]})
    chans = Channel.resolve(
        what=UnresolvedChannel.parse("known-forges"),
        platforms=platforms,
        channel_alias=channel_alias,
        custom_channels=custom_channels,
        custom_multichannels=custom_multichannels,
        authentication_db=auth_db,
        home_dir=home_dir,
        current_working_dir=cwd,
    )
    assert len(chans) == 2
    assert {c.display_name for c in chans} == {"best-forge", "conda-forge"}


def test_VersionPartAtom():
    VersionPartAtom = libmambapy.specs.VersionPartAtom

    a = VersionPartAtom(numeral=1, literal="alpha")

    # Getters
    assert a.numeral == 1
    assert a.literal == "alpha"
    assert str(a) == "1alpha"

    # Comparison
    b = VersionPartAtom(2)
    assert a == a
    assert a != b
    assert a <= a
    assert a <= b
    assert a < b
    assert a >= a
    assert b >= a
    assert b > a

    # Copy
    other = copy.deepcopy(a)
    assert other == a
    assert other is not a


def test_VersionPart():
    VersionPartAtom = libmambapy.specs.VersionPartAtom
    VersionPart = libmambapy.specs.VersionPart

    p = VersionPart([VersionPartAtom(1, "a"), VersionPartAtom(3)])
    assert len(p) == 2


def test_CommonVersion():
    VersionPartAtom = libmambapy.specs.VersionPartAtom
    VersionPart = libmambapy.specs.VersionPart
    CommonVersion = libmambapy.specs.CommonVersion

    p = VersionPart([VersionPartAtom(1, "a"), VersionPartAtom(3)])
    v = CommonVersion([p, p])
    assert len(v) == 2


def test_Version():
    VersionPartAtom = libmambapy.specs.VersionPartAtom
    VersionPart = libmambapy.specs.VersionPart
    CommonVersion = libmambapy.specs.CommonVersion
    Version = libmambapy.specs.Version

    # Static data
    assert isinstance(Version.epoch_delim, str)
    assert isinstance(Version.local_delim, str)
    assert isinstance(Version.part_delim, str)
    assert isinstance(Version.part_delim_alt, str)
    assert isinstance(Version.part_delim_special, str)

    # Parse
    v = Version.parse("3!1.3ab2.4+42.0alpha")

    # Getters
    assert v.epoch == 3
    assert v.version == CommonVersion(
        [
            VersionPart([VersionPartAtom(1)]),
            VersionPart([VersionPartAtom(3, "ab"), VersionPartAtom(2)]),
            VersionPart([VersionPartAtom(4)]),
        ]
    )
    assert v.local == CommonVersion(
        [
            VersionPart([VersionPartAtom(42)]),
            VersionPart([VersionPartAtom(0, "alpha")]),
        ]
    )

    # str
    assert str(v) == "3!1.3ab2.4+42.0alpha"
    assert v.str(level=1) == "3!1+42"

    # Copy
    other = copy.deepcopy(v)
    assert other == v
    assert other is not v

    # Comparison
    v1 = Version.parse("1.0.1")
    v2 = Version.parse("1.2.3alpha2")
    assert v1 == v1
    assert v1 != v2
    assert v1 <= v1
    assert v1 <= v2
    assert v2 >= v1
    assert v2 >= v2
    assert v2 > v1
    assert v1.starts_with(Version.parse("1.0"))
    assert not v1.starts_with(v2)
    assert v2.compatible_with(older=v1, level=1)
    assert not v2.compatible_with(older=v1, level=2)
    assert not v1.compatible_with(older=v2, level=1)


def test_VersionSpec():
    Version = libmambapy.specs.Version
    VersionSpec = libmambapy.specs.VersionSpec

    # Static data
    assert isinstance(VersionSpec.and_token, str)
    assert isinstance(VersionSpec.or_token, str)
    assert isinstance(VersionSpec.left_parenthesis_token, str)
    assert isinstance(VersionSpec.right_parenthesis_token, str)
    assert isinstance(VersionSpec.prefered_free_str, str)
    assert isinstance(VersionSpec.all_free_strs, list)
    assert isinstance(VersionSpec.starts_with_str, str)
    assert isinstance(VersionSpec.equal_str, str)
    assert isinstance(VersionSpec.not_equal_str, str)
    assert isinstance(VersionSpec.greater_str, str)
    assert isinstance(VersionSpec.greater_equal_str, str)
    assert isinstance(VersionSpec.less_str, str)
    assert isinstance(VersionSpec.less_equal_str, str)
    assert isinstance(VersionSpec.compatible_str, str)
    assert isinstance(VersionSpec.glob_suffix_str, str)
    assert isinstance(VersionSpec.glob_suffix_token, str)

    vs = VersionSpec.parse(">2.0,<3.0")

    assert not vs.contains(Version.parse("1.1"))
    assert vs.contains(Version.parse("2.1"))

    # str
    assert str(vs) == ">2.0,<3.0"

    # Copy, no easy comparison, this may not work for all specs
    other = copy.deepcopy(vs)
    assert str(other) == str(vs)
    assert other is not vs


def test_PackageInfo():
    PackageInfo = libmambapy.specs.PackageInfo
    NoArchType = libmambapy.specs.NoArchType

    pkg = PackageInfo(name="pkg", version="1.0", build_string="bld", build_number=2)

    assert pkg.name == "pkg"
    assert pkg.version == "1.0"
    assert pkg.build_string == "bld"
    assert pkg.build_number == 2

    # str
    assert str(pkg) == "pkg-1.0-bld"

    # from_url
    pkg = PackageInfo.from_url("https://repo.mamba.pm/conda-forge/linux-64/bar-5.1-xld.conda#01234")
    assert pkg.name == "bar"
    assert pkg.version == "5.1"
    assert pkg.build_string == "xld"
    assert pkg.md5 == "01234"

    # getters and setters
    pkg.name = "foo"
    assert pkg.name == "foo"
    pkg.version = "4.0"
    assert pkg.version == "4.0"
    pkg.build_string = "mybld"
    assert pkg.build_string == "mybld"
    pkg.build_number = 5
    assert pkg.build_number == 5
    pkg.noarch = "Generic"
    assert pkg.noarch == NoArchType.Generic
    pkg.channel = "conda-forge"
    assert pkg.channel == "conda-forge"
    pkg.package_url = "https://repo.mamba.pm/conda-forge/linux-64/foo-4.0-mybld.conda"
    assert pkg.package_url == "https://repo.mamba.pm/conda-forge/linux-64/foo-4.0-mybld.conda"
    pkg.subdir = "linux-64"
    assert pkg.subdir == "linux-64"
    pkg.filename = "foo-4.0-mybld.conda"
    assert pkg.filename == "foo-4.0-mybld.conda"
    pkg.license = "MIT"
    assert pkg.license == "MIT"
    pkg.size = 3200
    assert pkg.size == 3200
    pkg.timestamp = 4532
    assert pkg.timestamp == 4532
    pkg.sha256 = "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b"
    assert pkg.sha256 == "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b"
    pkg.md5 = "68b329da9893e34099c7d8ad5cb9c940"
    assert pkg.md5 == "68b329da9893e34099c7d8ad5cb9c940"
    pkg.track_features = ["mkl"]
    assert pkg.track_features == ["mkl"]
    pkg.depends = ["python>=3.7"]
    assert pkg.depends == ["python>=3.7"]
    pkg.constrains = ["pip>=2.1"]
    assert pkg.constrains == ["pip>=2.1"]

    # Equality
    assert PackageInfo() == PackageInfo()
    assert pkg == pkg
    assert pkg != PackageInfo()

    # Copy
    other = copy.deepcopy(pkg)
    assert other == pkg
    assert other is not pkg


def test_PackageInfo_V2Migrator():
    """Explicit migration help added from v1 to v2."""
    import libmambapy

    with pytest.raises(Exception, match=r"libmambapy\.specs"):
        libmambapy.PackageInfo()

    pkg = libmambapy.specs.PackageInfo()

    with pytest.raises(Exception, match=r"filename"):
        pkg.fn
    with pytest.raises(Exception, match=r"filename"):
        pkg.fn = "foo"

    with pytest.raises(Exception, match=r"package_url"):
        pkg.url
    with pytest.raises(Exception, match=r"package_url"):
        pkg.url = "https://repo.mamba.pm/conda-forge/linux-64/foo-4.0-mybld.conda"


def test_MatchSpec():
    MatchSpec = libmambapy.specs.MatchSpec

    ms = MatchSpec.parse("conda-forge::python=3.7=*pypy")

    # str
    assert str(ms) == "conda-forge::python=3.7[build='*pypy']"


def test_MatchSpec_V2Migrator():
    """Explicit migration help added from v1 to v2."""
    import libmambapy

    with pytest.raises(Exception, match=r"libmambapy\.specs"):
        libmambapy.MatchSpec()
