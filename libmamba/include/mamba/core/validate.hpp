// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_VALIDATE_HPP
#define MAMBA_CORE_VALIDATE_HPP

#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "mamba/core/timeref.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/validation/keys.hpp"

namespace mamba::validation
{
    [[nodiscard]] auto sha256sum(const fs::u8path& path) -> std::string_view;

    [[nodiscard]] auto md5sum(const fs::u8path& path) -> std::string_view;

    auto file_size(const fs::u8path& path, std::uintmax_t validation) -> bool;

    inline constexpr std::size_t MAMBA_SHA256_SIZE_HEX = 64;
    inline constexpr std::size_t MAMBA_SHA256_SIZE_BYTES = 32;
    inline constexpr std::size_t MAMBA_MD5_SIZE_HEX = 32;
    inline constexpr std::size_t MAMBA_MD5_SIZE_BYTES = 16;
    inline constexpr std::size_t MAMBA_ED25519_KEYSIZE_HEX = 64;
    inline constexpr std::size_t MAMBA_ED25519_KEYSIZE_BYTES = 32;
    inline constexpr std::size_t MAMBA_ED25519_SIGSIZE_HEX = 128;
    inline constexpr std::size_t MAMBA_ED25519_SIGSIZE_BYTES = 64;

    auto generate_ed25519_keypair(std::byte* pk, std::byte* sk) -> int;
    auto generate_ed25519_keypair() -> std::pair<
        std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>,
        std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>>;
    auto generate_ed25519_keypair_hex() -> std::pair<std::string, std::string>;

    auto sign(const std::string& data, const std::byte* sk, std::byte* signature) -> int;
    auto sign(const std::string& data, const std::string& sk, std::string& signature) -> int;

    auto ed25519_sig_hex_to_bytes(const std::string& sig_hex, int& error_code) noexcept
        -> std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES>;

    auto ed25519_key_hex_to_bytes(const std::string& key_hex, int& error_code) noexcept
        -> std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>;

    auto
    verify(const std::byte* data, std::size_t data_len, const std::byte* pk, const std::byte* signature)
        -> int;
    auto verify(const std::string& data, const std::byte* pk, const std::byte* signature) -> int;
    auto verify(const std::string& data, const std::string& pk_hex, const std::string& signature_hex)
        -> int;

    /**
     * Verify a GPG/PGP signature against the hash of the binary data and
     * the additional trailer added in V4 signature.
     * See RFC4880, section 5.2.4 https://datatracker.ietf.org/doc/html/rfc4880#section-5.2.4
     * This method assumes hash function to be SHA-256
     */
    auto verify_gpg_hashed_msg(const std::byte* data, const std::byte* pk, const std::byte* signature)
        -> int;
    auto
    verify_gpg_hashed_msg(const std::string& data, const std::byte* pk, const std::byte* signature)
        -> int;
    auto
    verify_gpg_hashed_msg(const std::string& data, const std::string& pk, const std::string& signature)
        -> int;

    /**
     * Verify a GPG/PGP signature against the binary data and
     * the additional trailer added in V4 signature.
     * See RFC4880, section 5.2.4 https://datatracker.ietf.org/doc/html/rfc4880#section-5.2.4
     * This method assumes hash function to be SHA-256
     */
    auto verify_gpg(
        const std::string& data,
        const std::string& gpg_v4_trailer,
        const std::string& pk,
        const std::string& signature
    ) -> int;

    void check_timestamp_metadata_format(const std::string& ts);

    /**
     * Base class for spec implementations.
     */
    class SpecBase
    {
    public:

        virtual ~SpecBase() = default;

        [[nodiscard]] auto version_str() const -> std::string;

        [[nodiscard]] virtual auto canonicalize(const nlohmann::json& j) const -> std::string;

        [[nodiscard]] auto compatible_prefix() const -> std::string;
        [[nodiscard]] auto upgrade_prefix() const -> std::vector<std::string>;

        [[nodiscard]] auto is_compatible(const fs::u8path& p) const -> bool;
        [[nodiscard]] auto is_compatible(const nlohmann::json& j) const -> bool;
        [[nodiscard]] auto is_compatible(const std::string& version) const -> bool;

        [[nodiscard]] auto is_upgrade(const nlohmann::json& j) const -> bool;
        [[nodiscard]] auto is_upgrade(const std::string& version) const -> bool;

        [[nodiscard]] virtual auto upgradable() const -> bool;

        [[nodiscard]] virtual auto json_key() const -> std::string = 0;
        [[nodiscard]] virtual auto expiration_json_key() const -> std::string = 0;

        [[nodiscard]] virtual auto signatures(const nlohmann::json& j) const
            -> std::set<RoleSignature>
            = 0;

    protected:

        SpecBase(std::string spec_version);

        [[nodiscard]] auto get_json_value(const nlohmann::json& j) const -> std::string;

    private:

        std::string m_spec_version;
    };

    auto operator==(const SpecBase& sv1, const SpecBase& sv2) -> bool;
    auto operator!=(const SpecBase& sv1, const SpecBase& sv2) -> bool;


    /**
     * Base class for role implementation.
     */
    class RoleBase
    {
    public:

        RoleBase(std::string type, std::shared_ptr<SpecBase> sv);

        virtual ~RoleBase() = 0;

        [[nodiscard]] auto type() const -> std::string;
        [[nodiscard]] auto spec_version() const -> SpecBase&;
        [[nodiscard]] auto version() const -> std::size_t;
        [[nodiscard]] auto file_ext() const -> std::string;
        [[nodiscard]] auto expires() const -> std::string;

        [[nodiscard]] auto expired(const TimeRef& time_reference) const -> bool;

        [[nodiscard]] auto roles() const -> std::set<std::string>;
        [[nodiscard]] auto signatures(const nlohmann::json& j) const -> std::set<RoleSignature>;

        [[nodiscard]] virtual auto self_keys() const -> RoleFullKeys = 0;
        [[nodiscard]] auto all_keys() const -> std::map<std::string, RoleFullKeys>;

        friend void to_json(nlohmann::json& j, const RoleBase* r);
        friend void from_json(const nlohmann::json& j, RoleBase* r);

    protected:

        [[nodiscard]] auto read_json_file(const fs::u8path& p, bool update = false) const
            -> nlohmann::json;

        /**
         * Check that a threshold of valid signatures is met
         * for the signed metadata of a role, using another
         * role keys (possibly the same).
         * Both signed and signatures metadata are contained
         * in 'data'.
         */
        void check_role_signatures(const nlohmann::json& data, const RoleBase& role);
        /**
         * Check that a threshold of valid signatures is met
         * for the signed metadata, using a set of keys.
         */
        void check_signatures(
            const std::string& signed_data,
            const std::set<RoleSignature>& signatures,
            const RoleFullKeys& keyring
        ) const;

        void set_spec_version(std::shared_ptr<SpecBase> sv);
        void set_expiration(const std::string& expires);

        // Forwarding to spec implementation
        [[nodiscard]] auto canonicalize(const nlohmann::json& j) const -> std::string;
        // Return the spec implementation
        [[nodiscard]] auto spec_impl() const -> std::shared_ptr<SpecBase>;

        // Mandatory roles defined by the current role
        [[nodiscard]] virtual auto mandatory_defined_roles() const -> std::set<std::string>;
        // Optional roles defined by the current role
        [[nodiscard]] virtual auto optionally_defined_roles() const -> std::set<std::string>;

        // Check role
        void check_expiration_format() const;
        void check_defined_roles(bool allow_any = false) const;

        std::map<std::string, RoleFullKeys> m_defined_roles;

    private:

        std::string m_internal_type;
        std::string m_type;
        std::shared_ptr<SpecBase> p_spec;
        std::size_t m_version = 1;
        std::string m_expires;
        std::string m_ext = "json";
    };

    // Forward declaration of RepoIndexChecker.
    class RepoIndexChecker;

    /**
     * 'root' role interface.
     */
    class RootRole : public RoleBase
    {
    public:

        ~RootRole() override = default;

        auto update(fs::u8path path) -> std::unique_ptr<RootRole>;
        auto update(nlohmann::json j) -> std::unique_ptr<RootRole>;

        auto possible_update_files() -> std::vector<fs::u8path>;

        virtual auto build_index_checker(
            Context& context,
            const TimeRef& time_reference,
            const std::string& url,
            const fs::u8path& cache_path
        ) const -> std::unique_ptr<RepoIndexChecker>
            = 0;

    protected:

        RootRole(std::shared_ptr<SpecBase> spec);

    private:

        virtual auto create_update(const nlohmann::json& j) -> std::unique_ptr<RootRole> = 0;
    };


    /**
     * Interface that performs validity checks
     * on a repository packages index.
     */
    class RepoIndexChecker
    {
    public:

        virtual ~RepoIndexChecker() = default;
        virtual void verify_index(const nlohmann::json& j) const = 0;
        virtual void verify_index(const fs::u8path& p) const = 0;
        virtual void verify_package(const nlohmann::json& signed_data, const nlohmann::json& signatures) const = 0;

    protected:

        RepoIndexChecker() = default;
    };


    /**
     * Perform security check against a repository
     * package index using cryptographic signatures.
     * Relies on multiple roles defined in TUF specification.
     */
    class RepoChecker
    {
    public:

        /**
         * Constructor.
         * @param base_url Repository base URL
         * @param ref_path Path to the reference directory, hosting trusted root metadata
         * @param cache_path Path to the cache directory
         */
        RepoChecker(Context& context, std::string base_url, fs::u8path ref_path, fs::u8path cache_path = "");

        // Forwarding to a ``RepoIndexChecker`` implementation
        void verify_index(const nlohmann::json& j) const;
        void verify_index(const fs::u8path& p) const;
        void
        verify_package(const nlohmann::json& signed_data, const nlohmann::json& signatures) const;

        void generate_index_checker();

        auto cache_path() -> const fs::u8path&;

        auto root_version() -> std::size_t;

    private:

        std::string m_base_url;
        std::size_t m_root_version = 0;
        fs::u8path m_ref_path;
        fs::u8path m_cache_path;
        Context& m_context;

        auto initial_trusted_root() -> fs::u8path;
        auto ref_root() -> fs::u8path;
        auto cached_root() -> fs::u8path;

        void persist_file(const fs::u8path& file_path);

        std::unique_ptr<RepoIndexChecker> p_index_checker;

        auto get_root_role(const TimeRef& time_reference) -> std::unique_ptr<RootRole>;
    };
}
#endif
