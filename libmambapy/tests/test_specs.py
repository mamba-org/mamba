import copy

import libmambapy


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
