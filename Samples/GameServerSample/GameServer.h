#pragma once

#include <GameNetworkServer.h>
#include <GameSessionManager.h>
#include <memory>
#include <unordered_map>
#include "Character.h"

namespace GameServerSample
{
    class GameServer
    {
    public:
        GameServer();
        ~GameServer();
        
        // Server lifecycle
        auto start(uint16_t port) -> std::tuple<bool, std::optional<std::string>>;
        auto stop() -> void;
        auto is_running() const -> bool;
        
        // Character management
        auto create_character(uint64_t player_id, const std::string& name) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto load_character(uint64_t player_id, uint64_t character_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto save_character(uint64_t player_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Player management
        auto on_player_connected(uint64_t player_id) -> void;
        auto on_player_disconnected(uint64_t player_id) -> void;
        
    private:
        std::unique_ptr<GameNetwork::GameNetworkServer> network_server_;
        std::unordered_map<uint64_t, std::unique_ptr<Character>> player_characters_;
        
        bool is_running_{false};
        
        // Message handlers
        auto handle_login_message(uint64_t player_id, const std::vector<uint8_t>& data) -> void;
        auto handle_movement_message(uint64_t player_id, const std::vector<uint8_t>& data) -> void;
        auto handle_action_message(uint64_t player_id, const std::vector<uint8_t>& data) -> void;
    };
} // namespace GameServerSample
