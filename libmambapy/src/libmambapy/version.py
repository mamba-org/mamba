version_info = ("2", "0", "7")
version_prerelease = "rc0"
__version__ = ".".join(map(str, version_info))
if version_prerelease != "":
    __version__ = f"{__version__}.{version_prerelease}"
