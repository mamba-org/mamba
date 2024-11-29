version_info = ("2", "0", "4")
version_prerelease = ""
__version__ = ".".join(map(str, version_info))
if version_prerelease != "":
    __version__ = "{}.{}".format(__version__, version_prerelease)
