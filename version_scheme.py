class version_info:
    major = ""
    minor = ""
    patch = ""
    pre_release = ""
    name = ""

    def __init__(self, version: str):
        if "-" in version:
            raise ValueError(
                "'{}' is not a valid version name : `-` is reserved for another usage in conda packages version names".format(
                    version
                )
            )

        VALID_VERSION_PRERELEASE_TYPES = ("alpha", "beta")
        version_fields = version.split(".")
        version_fields_count = len(version_fields)
        if version_fields_count < 3:
            raise ValueError(
                "'{}' is not a valid version name :  valid version scheme contains 3 or more dots-separated fields, the pre-release name starting with the 4th field (valid examples: 1.2.3, 0.1.2.alpha3, 0.1.2.alpha.3)".format(
                    version
                )
            )

        self.major = version_fields[0]
        self.minor = version_fields[1]
        self.patch = version_fields[2]
        self.pre_release = ""
        if version_fields_count > 3:
            # we assume here that all the additional dot-separated values are part of the pre-release name
            self.pre_release = ".".join(version_fields[3:])

        version_errors = []

        if not self.major.isdigit():
            version_errors.append("'{}' is not a valid major version number".format(self.major))
        if not self.minor.isdigit():
            version_errors.append("'{}' is not a valid minor version number".format(self.minor))
        if not self.patch.isdigit():
            version_errors.append("'{}' is not a valid patch version number".format(self.patch))

        if self.pre_release != "" and not self.pre_release.startswith(
            VALID_VERSION_PRERELEASE_TYPES
        ):
            version_errors.append(
                "'{}' is not a valid pre-release name, pre-release names must start with either : {} ".format(
                    self.pre_release, VALID_VERSION_PRERELEASE_TYPES
                )
            )

        if len(version_errors) > 0:
            error_message = "'{}' is not a valid version name:".format(version)
            for error in version_errors:
                error_message += "\n - {}".format(error)
            hint = (
                "examples of valid versions: 1.2.3, 0.1.2, 1.2.3.alpha0, 1.2.3.beta1, 3.4.5.beta.2"
            )
            error_message += "\n{}".format(hint)
            raise ValueError(error_message)

        self.name = version

    def __str__(self):
        return self.name
