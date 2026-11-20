// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SBOM_HPP
#define MAMBA_CORE_SBOM_HPP

#include <optional>
#include <string>
#include <string_view>

#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba::sbom
{
    /**
     * Build the package URL (PURL) of a conda package.
     *
     * The PURL follows the `CEP-49 specification
     * <https://github.com/conda/ceps/blob/main/cep-0049.md>`_.
     *
     * CEP-49 permits more than one valid PURL spelling for the same package: the split between the
     * `namespace` component and the `repository_url` qualifier may slide along the channel path
     * segments, provided their concatenation is the channel base URL. This function always emits
     * one deterministic canonical form per package: the channel is placed in the `namespace` only
     * when it is a single path segment relative to the default repository, otherwise the full
     * channel base URL is emitted as the `repository_url` qualifier. Consumers comparing this
     * output against PURLs from other producers should not compare strings directly, but resolve
     * each PURL to its channel base URL (concatenation of `repository_url` and `namespace`)
     * before comparing.
     *
     * The `subpath` component is intentionally omitted: it identifies a path within the
     * package (an archive member or extracted file, cf. CEP-49 "subpath component"),
     * not the package artifact itself.
     * A file-level PURL producer would populate it from the file path.
     */
    [[nodiscard]] auto make_conda_purl(const specs::PackageInfo& pkg) -> std::string;

    namespace detail
    {
        /**
         * Conda package archive type.
         */
        enum class ArchiveType
        {
            conda,
            tar_bz2,
        };

        /**
         * Return the string value for an archive type (`conda` or `tar.bz2`).
         */
        [[nodiscard]] constexpr auto archive_type_str(ArchiveType type) -> std::string_view
        {
            switch (type)
            {
                case ArchiveType::conda:
                    return "conda";
                case ArchiveType::tar_bz2:
                    return "tar.bz2";
            }
            return {};
        }

        /**
         * Return the conda archive type of a package.
         *
         * Returns `ArchiveType::conda` or `ArchiveType::tar_bz2` based on the package filename
         * or package URL extension, or `std::nullopt` if the archive type cannot be determined.
         */
        [[nodiscard]] auto archive_type(const specs::PackageInfo& pkg) -> std::optional<ArchiveType>;

        /** Return the canonical channel "home" URL, i.e. the base URL of the channel without the
         * package name, the platform segment, trailing slashes, the token, credentials, the query,
         * or the fragment.
         *
         * Returns `std::nullopt` if neither the package URL nor the channel provide a valid
         * URL with an explicit scheme.
         */
        [[nodiscard]] auto channel_home_url(const specs::PackageInfo& pkg)
            -> std::optional<specs::CondaURL>;

        /** Derive a PURL namespace from a canonical channel home URL.
         *
         * A channel is encoded in the PURL namespace only when it is relative
         * to the default repository (https://conda.anaconda.org) and is a single path segment
         * (an implementation choice in `mamba`).
         * The namespace must not be "defaults" or "local", nor a recognized channel subdir
         * name (following CEP-49).
         *
         * Returns `std::nullopt` for non-default hosts or invalid/multi-segment paths.
         * In those cases, the channel is represented via the `repository_url` qualifier instead.
         */
        [[nodiscard]] auto namespace_from_channel_home(const specs::CondaURL& home)
            -> std::optional<std::string>;

        /**
         * Return the PURL namespace of a conda package, if any.
         *
         * This is derived from `channel_home_url` and `namespace_from_channel_home` when the
         * package URL or channel provides a valid URL with an explicit scheme. If no home URL is
         * available, a bare channel name may be used as the namespace when it is a valid
         * single-segment name (not "defaults", "local", or a channel subdir name). Returns
         * `std::nullopt` when the channel should be expressed via the `repository_url`
         * qualifier instead.
         */
        [[nodiscard]] auto channel_namespace(const specs::PackageInfo& pkg)
            -> std::optional<std::string>;

        /**
         * Return the canonical repository base URL of a conda package.
         *
         * When `channel_home_url` is available, this is the home URL pretty printed without
         * credentials. If the package or channel URL cannot be parsed but the channel contains an
         * explicit scheme, the raw channel string (with trailing slashes removed) is returned.
         * Bare channel names are resolved relative to the default repository
         * (`https://conda.anaconda.org`). If the channel is empty, the default repository URL
         * is returned.
         */
        [[nodiscard]] auto channel_repository_url(const specs::PackageInfo& pkg) -> std::string;

        /**
         * Build the PURL qualifiers string for a conda package.
         *
         * Emits qualifiers as `?key=value&...`. Always includes `build_number`.
         * Included when present: `build`, `subdir`, `type`, `checksum` (sha256),
         * and `repository_url`.
         *
         * Qualifier order is not normative per CEP-49.
         */
        [[nodiscard]] auto
        collect_qualifiers(const specs::PackageInfo& pkg, std::optional<std::string_view> repository_url)
            -> std::string;
    }
}

#endif  // MAMBA_CORE_SBOM_HPP
