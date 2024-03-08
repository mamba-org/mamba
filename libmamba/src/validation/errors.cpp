// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/util/string.hpp"
#include "mamba/validation/errors.hpp"

namespace mamba::validation
{
    trust_error::trust_error(std::string_view message)
        : m_message(util::concat("Content trust error. ", message, ". Aborting."))
    {
    }

    auto trust_error::what() const noexcept -> const char*
    {
        return this->m_message.c_str();
    }

    threshold_error::threshold_error()
        : trust_error("Signatures threshold not met")
    {
    }

    role_metadata_error::role_metadata_error()
        : trust_error("Invalid role metadata")
    {
    }

    rollback_error::rollback_error()
        : trust_error("Possible rollback attack")
    {
    }

    freeze_error::freeze_error()
        : trust_error("Possible freeze attack")
    {
    }

    role_file_error::role_file_error()
        : trust_error("Invalid role file")
    {
    }

    spec_version_error::spec_version_error()
        : trust_error("Unsupported specification version")
    {
    }

    fetching_error::fetching_error()
        : trust_error("Failed to fetch role metadata")
    {
    }

    package_error::package_error()
        : trust_error("Invalid package")
    {
    }

    role_error::role_error()
        : trust_error("Invalid role")
    {
    }

    index_error::index_error()
        : trust_error("Invalid package index metadata")
    {
    }

    signatures_error::signatures_error()
        : trust_error("Invalid package signatures")
    {
    }
}
