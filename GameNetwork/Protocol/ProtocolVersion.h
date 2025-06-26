#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <string>
#include <tuple>

namespace GameNetwork
{
    namespace Protocol
    {
        // Protocol version information
        struct ProtocolVersion
        {
            uint32_t major;
            uint32_t minor;
            uint32_t patch;
            
            auto to_uint32() const -> uint32_t
            {
                return (major << 16) | (minor << 8) | patch;
            }
            
            static auto from_uint32(uint32_t version) -> ProtocolVersion
            {
                return {
                    (version >> 16) & 0xFF,
                    (version >> 8) & 0xFF,
                    version & 0xFF
                };
            }
            
            auto operator==(const ProtocolVersion& other) const -> bool
            {
                return major == other.major && minor == other.minor && patch == other.patch;
            }
            
            auto operator<(const ProtocolVersion& other) const -> bool
            {
                if (major != other.major) return major < other.major;
                if (minor != other.minor) return minor < other.minor;
                return patch < other.patch;
            }
        };
        
        // Version compatibility rules
        enum class CompatibilityLevel
        {
            FullyCompatible,      // Same version
            BackwardCompatible,   // Server can handle older client
            ForwardCompatible,    // Client can handle older server
            MajorMismatch,        // Incompatible major version
            Incompatible          // No compatibility
        };
        
        // Protocol version manager
        class ProtocolVersionManager
        {
        public:
            static constexpr ProtocolVersion CURRENT_VERSION = {1, 0, 0};
            static constexpr ProtocolVersion MIN_SUPPORTED_VERSION = {1, 0, 0};
            
            ProtocolVersionManager();
            ~ProtocolVersionManager();
            
            // Version negotiation
            auto negotiate_version(const ProtocolVersion& client_version) 
                -> std::tuple<bool, ProtocolVersion, std::optional<std::string>>;
            
            // Compatibility check
            auto check_compatibility(const ProtocolVersion& client_version) -> CompatibilityLevel;
            
            // Feature availability
            auto is_feature_available(const std::string& feature_name, const ProtocolVersion& version) -> bool;
            
            // Version-specific handlers
            using VersionHandler = std::function<void(const ProtocolVersion&)>;
            auto register_version_handler(const ProtocolVersion& version, VersionHandler handler) -> void;
            
            // Get version info
            auto get_current_version() const -> ProtocolVersion { return CURRENT_VERSION; }
            auto get_min_supported_version() const -> ProtocolVersion { return MIN_SUPPORTED_VERSION; }
            auto get_supported_versions() const -> std::vector<ProtocolVersion>;
            
            // Protocol capability negotiation
            struct ProtocolCapabilities
            {
                bool supports_compression;
                bool supports_encryption;
                bool supports_reliable_udp;
                bool supports_batch_messages;
                bool supports_delta_compression;
                uint32_t max_packet_size;
                uint32_t max_message_rate;
            };
            
            auto get_capabilities(const ProtocolVersion& version) const -> ProtocolCapabilities;
            
        private:
            struct VersionInfo
            {
                ProtocolVersion version;
                ProtocolCapabilities capabilities;
                VersionHandler handler;
                std::vector<std::string> features;
            };
            
            std::unordered_map<uint32_t, VersionInfo> version_registry_;
            
            auto register_default_versions() -> void;
        };
        
        // Protocol handshake messages
        struct HandshakeRequest
        {
            ProtocolVersion client_version;
            std::vector<std::string> supported_features;
            std::string client_id;
            uint64_t timestamp;
        };
        
        struct HandshakeResponse
        {
            bool accepted;
            ProtocolVersion negotiated_version;
            ProtocolVersionManager::ProtocolCapabilities capabilities;
            std::string session_id;
            std::optional<std::string> rejection_reason;
        };
        
    } // namespace Protocol
} // namespace GameNetwork
