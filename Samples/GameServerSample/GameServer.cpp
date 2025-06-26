#include "GameServer.h"
#include <Logger.h>

namespace GameServerSample
{
    GameServer::GameServer() = default;
    GameServer::~GameServer() 
    {
        stop();
    }
    
    auto GameServer::start(uint16_t port) -> std::tuple<bool, std::optional<std::string>>
    {
        if (is_running_)
        {
            return {false, "Server is already running"};
        }
        
        // Configure network server
        GameNetwork::ServerConfig config;
        config.server_id = "GameServerSample";
        config.server_name = "Sample Game Server";
        config.port = port;
        config.max_players = 1000;
        config.max_channels = 10;
        config.enable_encryption = false;
        config.enable_compression = true;
        
        // Create network server
        network_server_ = std::make_unique<GameNetwork::GameNetworkServer>(config);
        
        // Register message handlers
        network_server_->on_client_connected([this](const std::string& session_id) {
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Player connected: " + session_id);
            // Convert session_id to player_id (simplified)
            uint64_t player_id = std::hash<std::string>{}(session_id);
            on_player_connected(player_id);
        });
        
        network_server_->on_client_disconnected([this](const std::string& session_id) {
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Player disconnected: " + session_id);
            uint64_t player_id = std::hash<std::string>{}(session_id);
            on_player_disconnected(player_id);
        });
        
        // Start the server
        auto result = network_server_->start();
        if (std::get<0>(result))
        {
            is_running_ = true;
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Game server started on port " + std::to_string(port));
        }
        
        return result;
    }
    
    auto GameServer::stop() -> void
    {
        if (!is_running_)
        {
            return;
        }
        
        // Save all characters before stopping
        for (const auto& [player_id, character] : player_characters_)
        {
            character->save();
        }
        
        network_server_->stop();
        network_server_.reset();
        player_characters_.clear();
        
        is_running_ = false;
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Game server stopped");
    }
    
    auto GameServer::is_running() const -> bool
    {
        return is_running_;
    }
    
    auto GameServer::create_character(uint64_t player_id, const std::string& name) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        auto character = std::make_unique<Character>();
        // In real implementation, would generate unique character_id from database
        uint64_t character_id = player_id; // Simplified
        
        // Set character data
        // character->set_name(name); // Would need to add this method
        
        player_characters_[player_id] = std::move(character);
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Character created for player " + std::to_string(player_id));
        
        return {true, std::nullopt};
    }
    
    auto GameServer::load_character(uint64_t player_id, uint64_t character_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        auto character = std::make_unique<Character>();
        
        // Load character from database
        auto result = character->load(character_id, std::to_string(player_id));
        if (!std::get<0>(result))
        {
            return result;
        }
        
        player_characters_[player_id] = std::move(character);
        
        return {true, std::nullopt};
    }
    
    auto GameServer::save_character(uint64_t player_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        auto it = player_characters_.find(player_id);
        if (it == player_characters_.end())
        {
            return {false, "Character not found for player"};
        }
        
        return it->second->save();
    }
    
    auto GameServer::on_player_connected(uint64_t player_id) -> void
    {
        // In real implementation, would load character from database
        load_character(player_id, player_id);
    }
    
    auto GameServer::on_player_disconnected(uint64_t player_id) -> void
    {
        // Save character and remove from memory
        save_character(player_id);
        player_characters_.erase(player_id);
    }
    
    auto GameServer::handle_login_message(uint64_t player_id, const std::vector<uint8_t>& data) -> void
    {
        // Handle login logic
        // This is where character selection/creation would happen
    }
    
    auto GameServer::handle_movement_message(uint64_t player_id, const std::vector<uint8_t>& data) -> void
    {
        auto it = player_characters_.find(player_id);
        if (it != player_characters_.end())
        {
            // Parse movement data and update character location
            // Location new_loc = parse_location(data);
            // it->second->set_location(new_loc);
        }
    }
    
    auto GameServer::handle_action_message(uint64_t player_id, const std::vector<uint8_t>& data) -> void
    {
        // Handle player actions (combat, interaction, etc.)
    }
    
} // namespace GameServerSample
