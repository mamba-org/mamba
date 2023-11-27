import copy

import pytest

import libmambapy


def test_platform():
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
        Platform("linux-64") == Platform.linux_64


def test_conda_url_credentials():
    Credentials = libmambapy.specs.CondaURL.Credentials

    assert Credentials.Hide.name == "Hide"
    assert Credentials.Show.name == "Show"
    assert Credentials.Remove.name == "Remove"
    assert Credentials("Show") == Credentials.Show


def test_conda_url_setters():
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


def test_conda_url_parse():
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


def test_conda_url_op():
    CondaURL = libmambapy.specs.CondaURL
    url = CondaURL.parse(
        "https://user%40mail.com:pas%23@repo.mamba.pm:400/t/xy-12345678-1234/%20conda/linux-64/pkg.conda"
    )

    #  Copy
    other = copy.deepcopy(url)
    assert other.str() == url.str()

    # Comparison
    assert hash(url) != 0
    assert url == url
    assert other == url
    other.set_host("somehost.com")
    assert other != url


def test_channel_spec_type():
    Type = libmambapy.specs.ChannelSpec.Type

    assert Type.URL.name == "URL"
    assert Type.PackageURL.name == "PackageURL"
    assert Type.Path.name == "Path"
    assert Type.PackagePath.name == "PackagePath"
    assert Type.Name.name == "Name"
    assert Type.Unknown.name == "Unknown"
    assert Type("Name").name == "Name"


def test_channel_spec():
    ChannelSpec = libmambapy.specs.ChannelSpec

    # Constructor
    spec = ChannelSpec(
        location="<unknown>",
        platform_filters=set(),
        type=ChannelSpec.Type.Unknown,
    )
    assert spec.location == "<unknown>"
    assert spec.platform_filters == set()
    assert spec.type == ChannelSpec.Type.Unknown

    # Enum cast
    spec = ChannelSpec(location="conda-forge", platform_filters=set(), type="Name")
    assert spec.type == ChannelSpec.Type.Name

    #  Parser
    spec = ChannelSpec.parse("conda-forge[linux-64]")
    assert spec.location == "conda-forge"
    assert spec.platform_filters == {"linux-64"}
    assert spec.type == ChannelSpec.Type.Name

    #  Setters
    spec.location = "https://mamba.pm/"
    assert spec.location == "https://mamba.pm/"
    spec.platform_filters = {"win-64"}
    assert spec.platform_filters == {"win-64"}
    spec.type = ChannelSpec.Type.URL
    assert spec.type == ChannelSpec.Type.URL

    #  Copy
    other = copy.deepcopy(spec)
    assert other.location == spec.location


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

    # Comparion
    assert auth == auth
    assert auth == other
    other.token = "foo"
    assert auth != other
    assert hash(auth) != 0
