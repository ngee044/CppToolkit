#include "ProtocolVersion.h"

#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <algorithm>

using namespace Utilities;

namespace GameNetwork
{
    namespace Protocol
    {
        ProtocolVersionManager::ProtocolVersionManager()
        {
            register_default_versions();
        }
        
        ProtocolVersionManager::~ProtocolVersionManager() = default;
        
        auto ProtocolVersionManager::negotiate_version(const ProtocolVersion& client_version) 
            -> std::tuple<bool, ProtocolVersion, std::optional<std::string>>
        {
            auto compatibility = check_compatibility(client_version);
            
            switch (compatibility)
            {
                case CompatibilityLevel::FullyCompatible:
                    return {true, client_version, std::nullopt};
                    
                case CompatibilityLevel::BackwardCompatible:
                    return {true, client_version, std::nullopt};
                    
                case CompatibilityLevel::ForwardCompatible:
                    return {true, CURRENT_VERSION, std::nullopt};
                    
                case CompatibilityLevel::MajorMismatch:
                    return {false, CURRENT_VERSION, "Major version mismatch. Client: " + 
                            std::to_string(client_version.major) + ", Server: " + 
                            std::to_string(CURRENT_VERSION.major)};
                    
                case CompatibilityLevel::Incompatible:
                default:
                    return {false, CURRENT_VERSION, "Protocol version incompatible"};
            }
        }
        
        auto ProtocolVersionManager::check_compatibility(const ProtocolVersion& client_version) -> CompatibilityLevel
        {
            // Same version
            if (client_version == CURRENT_VERSION)
            {
                return CompatibilityLevel::FullyCompatible;
            }
            
            // Major version must match
            if (client_version.major != CURRENT_VERSION.major)
            {
                return CompatibilityLevel::MajorMismatch;
            }
            
            // Check if version is in supported range
            if (client_version < MIN_SUPPORTED_VERSION)
            {
                return CompatibilityLevel::Incompatible;
            }
            
            // Minor version differences
            if (client_version.minor < CURRENT_VERSION.minor)
            {
                return CompatibilityLevel::BackwardCompatible;
            }
            else if (client_version.minor > CURRENT_VERSION.minor)
            {
                return CompatibilityLevel::ForwardCompatible;
            }
            
            // Patch differences are always compatible
            return CompatibilityLevel::BackwardCompatible;
        }
        
        auto ProtocolVersionManager::is_feature_available(const std::string& feature_name, const ProtocolVersion& version) -> bool
        {
            auto version_key = version.to_uint32();
            auto it = version_registry_.find(version_key);
            
            if (it != version_registry_.end())
            {
                const auto& features = it->second.features;
                return std::find(features.begin(), features.end(), feature_name) != features.end();
            }
            
            return false;
        }
        
        auto ProtocolVersionManager::register_version_handler(const ProtocolVersion& version, VersionHandler handler) -> void
        {
            auto version_key = version.to_uint32();
            version_registry_[version_key].handler = handler;
        }
        
        auto ProtocolVersionManager::get_supported_versions() const -> std::vector<ProtocolVersion>
        {
            std::vector<ProtocolVersion> versions;
            for (const auto& [key, info] : version_registry_)
            {
                versions.push_back(info.version);
            }
            
            std::sort(versions.begin(), versions.end());
            return versions;
        }
        
        auto ProtocolVersionManager::get_capabilities(const ProtocolVersion& version) const 
            -> ProtocolCapabilities
        {
            auto version_key = version.to_uint32();
            auto it = version_registry_.find(version_key);
            
            if (it != version_registry_.end())
            {
                return it->second.capabilities;
            }
            
            // Return minimal capabilities for unknown versions
            return {
                false,  // supports_compression
                false,  // supports_encryption
                false,  // supports_reliable_udp
                false,  // supports_batch_messages
                false,  // supports_delta_compression
                4096,   // max_packet_size
                100     // max_message_rate
            };
        }
        
        auto ProtocolVersionManager::register_default_versions() -> void
        {
            // Version 1.0.0 - Initial release
            VersionInfo v1_0_0;
            v1_0_0.version = {1, 0, 0};
            v1_0_0.capabilities = {
                true,   // supports_compression
                true,   // supports_encryption
                true,   // supports_reliable_udp
                true,   // supports_batch_messages
                false,  // supports_delta_compression (added in 1.1.0)
                65536,  // max_packet_size
                1000    // max_message_rate
            };
            v1_0_0.features = {
                "basic_messaging",
                "session_management",
                "channel_system",
                "load_balancing"
            };
            
            version_registry_[v1_0_0.version.to_uint32()] = v1_0_0;

            Logger::handle().write(LogTypes::Information,
                fmt::format("Protocol version manager initialized with version {}.{}.{}",
                            CURRENT_VERSION.major,
                            CURRENT_VERSION.minor,
                            CURRENT_VERSION.patch));
        }
        
    } // namespace Protocol
} // namespace GameNetwork
