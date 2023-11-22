import copy

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


def test_channel_spec_type():
    Type = libmambapy.specs.ChannelSpec.Type

    assert Type.URL.name == "URL"
    assert Type.PackageURL.name == "PackageURL"
    assert Type.Path.name == "Path"
    assert Type.PackagePath.name == "PackagePath"
    assert Type.Name.name == "Name"
    assert Type.Unknown.name == "Unknown"


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
