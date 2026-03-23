#pragma once

#include "RedisConnector.h"

#include <expected>
#include <string>
#include <optional>
#include <vector>
#include <utility>

#include <format>

namespace Redis
{
	class RedisClient
	{
	public:
		RedisClient(const std::string& address, const int& port = 6379, const TLSOptions& tls_options = TLSOptions(), const int& db_index = 0);
		~RedisClient(void);

		auto connect(void) -> std::expected<void, std::string>;
		auto is_connected(void) const -> bool;
		auto disconnect(void) -> std::expected<void, std::string>;

		auto set(const std::string& key, const std::string& value, long ttl_seconds = 0) -> std::expected<void, std::string>;
		auto get(const std::string& key) -> std::expected<std::string, std::string>;

		auto lpush(const std::string& key, const std::vector<std::string>& values, long ttl_seconds = 0) -> std::expected<long long, std::string>;
		auto rpush(const std::string& key, const std::vector<std::string>& values, long ttl_seconds = 0) -> std::expected<long long, std::string>;
		auto lpop(const std::string& key) -> std::expected<std::optional<std::string>, std::string>;
		auto rpop(const std::string& key) -> std::expected<std::optional<std::string>, std::string>;
		auto lrange(const std::string& key, long start = 0, long stop = -1) -> std::expected<std::vector<std::string>, std::string>;

		auto blpop(const std::string& key,
				   const std::optional<long>& timeout_seconds = std::nullopt) -> std::expected<std::optional<std::string>, std::string>;

		auto zadd(const std::string& key,
				  const std::vector<std::pair<std::string, double>>& members,
				  long ttl_seconds = 0) -> std::expected<long long, std::string>;
		auto zrange(const std::string& key, long start = 0, long stop = -1, bool with_scores = false) -> std::expected<std::vector<std::string>, std::string>;
		auto zrem(const std::string& key, const std::vector<std::string>& members) -> std::expected<long long, std::string>;

		auto set_ttl(const std::string& key, long ttl_seconds) -> std::expected<void, std::string>;

		auto llen(const std::string& key) -> std::expected<long long, std::string>;
		auto del(const std::string& key) -> std::expected<long long, std::string>;

		auto lrem(const std::string& key, long count, const std::string& value) -> std::expected<long long, std::string>;

		auto xadd(const std::string& key,
				  const std::map<std::string, std::string>& fields,
				  const std::optional<std::string>& id = std::nullopt,
				  long maxlen = 0,
				  long ttl_seconds = 0) -> std::expected<std::string, std::string>;

		auto xread(const std::vector<std::string>& keys, const std::vector<std::string>& ids, long count = 0, long block = 0)
			-> std::expected<std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>, std::string>;

		auto xlen(const std::string& key) -> std::expected<long long, std::string>;

		auto xdel(const std::string& key, const std::vector<std::string>& ids) -> std::expected<long long, std::string>;

		auto xrange(const std::string& key,
					const std::string& start = "-",
					const std::string& end = "+",
					long count = 0) -> std::expected<std::vector<std::pair<std::string, std::map<std::string, std::string>>>, std::string>;

		auto xrevrange(const std::string& key,
					   const std::string& end = "+",
					   const std::string& start = "-",
					   long count = 0) -> std::expected<std::vector<std::pair<std::string, std::map<std::string, std::string>>>, std::string>;

		auto xgroup_create(const std::string& key,
						   const std::string& group_name,
						   const std::string& id = "$",
						   bool mkstream = false) -> std::expected<void, std::string>;

		auto xgroup_destroy(const std::string& key, const std::string& group_name) -> std::expected<void, std::string>;

		auto xreadgroup(const std::string& group_name,
						const std::string& consumer_name,
						const std::vector<std::string>& keys,
						const std::vector<std::string>& ids,
						long count = 0,
						long block = 0)
			-> std::expected<std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>, std::string>;

		auto xack(const std::string& key, const std::string& group_name, const std::vector<std::string>& ids) -> std::expected<long long, std::string>;

		template <typename TansactionFunction>
		auto transaction_functions(TansactionFunction&& transaction_function) -> std::expected<sw::redis::QueuedReplies, std::string>
		{
			if (connector_ == nullptr)
			{
				return std::unexpected("Connector is not created.");
			}

			if (!connector_->is_connected())
			{
				auto connect_result = connector_->connect();
				if (!connect_result.has_value())
				{
					return std::unexpected(std::format("failed to start transaction: {}", connect_result.error()));
				}
			}

			auto transaction = connector_->get_transaction();
			if (transaction == nullptr)
			{
				return std::unexpected("failed to get redis connection.");
			}

			try
			{
				transaction_function(transaction);

				return transaction->exec();
			}
			catch (const sw::redis::Error& err)
			{
				connector_->disconnect();

				return std::unexpected(std::format("failed to execute transaction: {}", err.what()));
			}
		}

	private:
		std::shared_ptr<RedisConnector> connector_;
	};
}
