#pragma once

#include "TLSOptions.h"

#include <sw/redis++/redis++.h>

#include <tuple>
#include <string>
#include <memory>
#include <optional>

namespace Redis
{
	class RedisConnector
	{
	public:
		RedisConnector(const std::string& host, int port = 6379, const TLSOptions& tlsOptions = TLSOptions(), const int& db_index = 0);
		~RedisConnector();

		auto connect() -> std::tuple<bool, std::optional<std::string>>;
		auto disconnect() -> std::tuple<bool, std::optional<std::string>>;
		auto is_connected() const -> bool;

		auto get_redis() const -> std::shared_ptr<sw::redis::Redis>;
		auto get_transaction() const -> std::shared_ptr<sw::redis::Transaction>;


	private:
		std::string host_;
		int port_;
		TLSOptions tlsOptions_;
		int db_index_;

		std::shared_ptr<sw::redis::Redis> redis_;
		sw::redis::ConnectionOptions connection_options_;
	};
}