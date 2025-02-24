#pragma once

#include "RedisConnector.h"

#include <string>
#include <tuple>
#include <optional>
#include <vector>
#include <utility>

namespace Redis
{
	class RedisClient
	{
	public:
		RedisClient(const std::string& host, const int& port, const TLSOptions& tls_options = TLSOptions(), const int& db_index = 0);
		~RedisClient();

		auto connect() -> std::tuple<bool, std::optional<std::string>>;
		auto is_connected() const -> bool;
		auto disconnect() -> std::tuple<bool, std::optional<std::string>>;

		auto set(const std::string& key, const std::string& value, std::uint32_t ttl_sec = 0) -> std::tuple<bool, std::optional<std::string>>;
		auto get(const std::string& key) -> std::tuple<std::string, std::optional<std::string>>;

		auto set_ttl(const std::string& key, std::uint32_t ttl_sec) -> std::tuple<bool, std::optional<std::string>>;

	private:
		std::shared_ptr<RedisConnector> redis_connector_;
	};
}