version_info = ("2", "3", "3")
version_prerelease = "alpha1"
__version__ = ".".join(map(str, version_info))
if version_prerelease != "":
    __version__ = f"{__version__}.{version_prerelease}"
