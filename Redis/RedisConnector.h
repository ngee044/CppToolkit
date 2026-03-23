#pragma once

#include "TLSOptions.h"

#include <sw/redis++/redis++.h>

#include <expected>
#include <memory>
#include <string>

namespace Redis
{
	class RedisConnector
	{
	public:
		RedisConnector(const std::string& address, const int& port = 6379, const TLSOptions& tls_options = TLSOptions(), const int& db_index = 0);
		~RedisConnector(void);

		auto connect(void) -> std::expected<void, std::string>;
		auto disconnect(void) -> std::expected<void, std::string>;
		auto is_connected(void) const -> bool;

		auto get_redis(void) const -> std::shared_ptr<sw::redis::Redis>;
		auto get_transaction(void) const -> std::shared_ptr<sw::redis::Transaction>;

	private:
		std::string address_;
		int port_;
		TLSOptions tls_options_;
		int db_index_;
		std::shared_ptr<sw::redis::Redis> redis_;
		sw::redis::ConnectionOptions connection_options_;
	};
}
