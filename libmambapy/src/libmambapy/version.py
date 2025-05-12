version_info = ("2", "1", "1")
version_prerelease = ""
__version__ = ".".join(map(str, version_info))
if version_prerelease != "":
    __version__ = f"{__version__}.{version_prerelease}"
