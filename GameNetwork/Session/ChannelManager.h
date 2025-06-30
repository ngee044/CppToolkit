#pragma once

#include <GameSession.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace GameNetwork
{
	class ChannelManager
	{
	public:
		ChannelManager(uint32_t max_channels);
		~ChannelManager();
        
		// Channel operations
		auto join_channel(uint32_t channel_id, std::shared_ptr<GameSession> session) 
			-> std::tuple<bool, std::optional<std::string>>;
		auto leave_channel(uint32_t channel_id, std::shared_ptr<GameSession> session) 
			-> std::tuple<bool, std::optional<std::string>>;
        
		// Channel queries
		auto get_channel_sessions(uint32_t channel_id) const 
			-> std::vector<std::shared_ptr<GameSession>>;
		auto get_channel_count(uint32_t channel_id) const -> size_t;
		auto is_channel_full(uint32_t channel_id) const -> bool;
        
	private:
		mutable std::mutex mutex_;
		uint32_t max_channels_;
		uint32_t max_players_per_channel_;
		std::unordered_map<uint32_t, std::vector<std::shared_ptr<GameSession>>> channels_;
	};
}
