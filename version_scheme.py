# Parses and validates the version scheme chosen for mamba versions.
# Specifically, we use a dot to separate pre-release names and use complete names for `alpha` and `beta`.
#
# See:
#  - discussion in https://github.com/mamba-org/mamba/issues/3638
#  - https://conda-forge.org/docs/maintainer/knowledge_base/#pre-release-version-sorting
#  - https://github.com/conda/conda/blob/cc21508563912268649f207723fd5114fa21b906/conda/models/version.py#L115-L143
class version_info:
    major = ""
    minor = ""
    patch = ""
    pre_release = ""
    name = ""

    def __init__(self, version: str):
        if not isinstance(version, str):
            raise ValueError(f"'{version}' is not a valid version name : must be a string")

        if "-" in version:
            raise ValueError(
                f"'{version}' is not a valid version name : `-` is reserved for another usage in conda packages version names"
            )

        VALID_VERSION_PRERELEASE_TYPES = ("alpha", "beta", "rc", "dev")
        version_fields = version.split(".")
        version_fields_count = len(version_fields)
        if version_fields_count < 3:
            raise ValueError(
                f"'{version}' is not a valid version name :  valid version scheme contains 3 or more dots-separated fields, the pre-release name starting with the 4th field (valid examples: 1.2.3, 0.1.2.alpha3, 0.1.2.alpha.3)"
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
            version_errors.append(f"'{self.major}' is not a valid major version number")
        if not self.minor.isdigit():
            version_errors.append(f"'{self.minor}' is not a valid minor version number")
        if not self.patch.isdigit():
            version_errors.append(f"'{self.patch}' is not a valid patch version number")

        if self.pre_release != "" and not self.pre_release.startswith(
            VALID_VERSION_PRERELEASE_TYPES
        ):
            version_errors.append(
                f"'{self.pre_release}' is not a valid pre-release name, pre-release names must start with either : {VALID_VERSION_PRERELEASE_TYPES} "
            )

        if len(version_errors) > 0:
            error_message = f"'{version}' is not a valid version name:"
            for error in version_errors:
                error_message += f"\n - {error}"
            hint = (
                "examples of valid versions: 1.2.3, 0.1.2, 1.2.3.alpha0, 1.2.3.beta1, 3.4.5.beta.2"
            )
            error_message += f"\n{hint}"
            raise ValueError(error_message)

        self.name = version

    def __str__(self):
        return self.name
