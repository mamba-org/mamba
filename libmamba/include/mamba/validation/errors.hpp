// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_VALIDATION_ERRORS_HPP
#define MAMBA_VALIDATION_ERRORS_HPP

#include <exception>
#include <string>
#include <string_view>

namespace mamba::validation
{
    /**
     * Base class for artifact/package verification error.
     */
    class trust_error : public std::exception
    {
    public:

        trust_error(std::string_view message);
        ~trust_error() override = default;
        [[nodiscard]] auto what() const noexcept -> const char* override;

    private:

        std::string m_message;
    };

    /**
     * Error raised when a threshold of signatures is not met.
     *
     * This can be due to wrong signatures, wrong or missing public keys.
     */
    class threshold_error : public trust_error
    {
    public:

        threshold_error();
        ~threshold_error() override = default;
    };

    /**
     * Error raised when wrong metadata are spotted
     * in a role file.
     */
    class role_metadata_error : public trust_error
    {
    public:

        role_metadata_error();
        ~role_metadata_error() override = default;
    };

    /**
     * Error raised when a wrong file name is
     * detected for role metadata.
     */
    class role_file_error : public trust_error
    {
    public:

        role_file_error();
        ~role_file_error() override = default;
    };

    /**
     * Error raised when a possible rollback attack is detected.
     */
    class rollback_error : public trust_error
    {
    public:

        rollback_error();
        ~rollback_error() override = default;
    };

    /**
     * Error raised when a possible freeze attack is detected.
     */
    class freeze_error : public trust_error
    {
    public:

        freeze_error();
        ~freeze_error() override = default;
    };

    /**
     * Error raised when a spec version is either wrong/invalid or not supported by the client.
     */
    class spec_version_error : public trust_error
    {
    public:

        spec_version_error();
        ~spec_version_error() override = default;
    };

    /**
     * Error raised when a role metadata file fetching process fails.
     */
    class fetching_error : public trust_error
    {
    public:

        fetching_error();
        ~fetching_error() override = default;
    };

    /**
     * Error raised when signatures threshold is not met for a package.
     */
    class package_error : public trust_error
    {
    public:

        package_error();
        ~package_error() override = default;
    };

    /**
     * Error raised when signatures threshold is not met for a trust role.
     */
    class role_error : public trust_error
    {
    public:

        role_error();
        ~role_error() override = default;
    };

    /**
     * Error raised when an invalid package index is met.
     */
    class index_error : public trust_error
    {
    public:

        index_error();
        ~index_error() override = default;
    };

    /**
     * Error raised when the given signatures of a package are empty/invalid.
     */
    class signatures_error : public trust_error
    {
    public:

        signatures_error();
        ~signatures_error() override = default;
    };
}
#endif
