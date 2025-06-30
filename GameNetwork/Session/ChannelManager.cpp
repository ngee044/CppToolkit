#include "ChannelManager.h"

#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <algorithm>

using namespace Utilities;

namespace GameNetwork
{
	ChannelManager::ChannelManager(uint32_t max_channels)
		: max_channels_(max_channels)
		, max_players_per_channel_(100) // Default
	{
	}

	ChannelManager::~ChannelManager() = default;

	auto ChannelManager::join_channel(uint32_t channel_id, std::shared_ptr<GameSession> session) 
		-> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (channel_id == 0 || channel_id > max_channels_)
		{
			return { false, "Invalid channel ID" };
		}
        
		auto& channel = channels_[channel_id];
        
		if (channel.size() >= max_players_per_channel_)
		{
			return { false, "Channel is full" };
		}
        
		// Check if already in channel
		auto it = std::find(channel.begin(), channel.end(), session);
		if (it != channel.end())
		{
			return { false, "Already in channel" };
		}
        
		channel.push_back(session);

		Logger::handle().write(LogTypes::Information,
			fmt::format("Session {} joined channel {}", session->session_id(), channel_id));

		return { true, std::nullopt };
	}

	auto ChannelManager::leave_channel(uint32_t channel_id, std::shared_ptr<GameSession> session) 
		-> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto channel_it = channels_.find(channel_id);
		if (channel_it == channels_.end())
		{
			return { false, "Channel not found" };
		}
        
		auto& channel = channel_it->second;
		auto session_it = std::find(channel.begin(), channel.end(), session);
        
		if (session_it == channel.end())
		{
			return { false, "Not in channel" };
		}
        
		channel.erase(session_it);
        
		// Remove empty channel
		if (channel.empty())
		{
			channels_.erase(channel_it);
		}
        
		return { true, std::nullopt };
	}

	auto ChannelManager::get_channel_sessions(uint32_t channel_id) const 
		-> std::vector<std::shared_ptr<GameSession>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = channels_.find(channel_id);
		if (it != channels_.end())
		{
			return it->second;
		}
        
		return {};
	}

	auto ChannelManager::get_channel_count(uint32_t channel_id) const -> size_t
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = channels_.find(channel_id);
		if (it != channels_.end())
		{
			return it->second.size();
		}
        
		return 0;
	}

	auto ChannelManager::is_channel_full(uint32_t channel_id) const -> bool
	{
		return get_channel_count(channel_id) >= max_players_per_channel_;
	}
}
