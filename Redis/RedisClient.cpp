// RedisClient.cpp
#include "RedisClient.h"

#include <format>
#include <iostream>

namespace Redis
{
	RedisClient::RedisClient(const std::string& address, const int& port, const TLSOptions& tls_options, const int& db_index)
		: connector_(std::make_shared<RedisConnector>(address, port, tls_options, db_index))
	{
	}

	RedisClient::~RedisClient() { connector_.reset(); }

	auto RedisClient::connect() -> std::expected<void, std::string>
	{
		if (connector_ == nullptr)
		{
			return std::unexpected("Connector is not created.");
		}

		return connector_->connect();
	}

	auto RedisClient::is_connected() const -> bool
	{
		if (connector_ == nullptr)
		{
			return false;
		}

		return connector_->is_connected();
	}

	auto RedisClient::disconnect() -> std::expected<void, std::string>
	{
		if (connector_ == nullptr)
		{
			return std::unexpected("Connector is not created.");
		}

		return connector_->disconnect();
	}

	auto RedisClient::set(const std::string& key, const std::string& value, long ttl_seconds) -> std::expected<void, std::string>
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
				return std::unexpected(std::format("failed to set value: {}", connect_result.error()));
			}
		}

		auto transaction = connector_->get_transaction();
		if (transaction == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			transaction->set(key, value);
			if (ttl_seconds > 0)
			{
				transaction->expire(key, ttl_seconds);
			}
			auto results = transaction->exec();

			if (!results.get<bool>(0))
			{
				return std::unexpected("failed to set value.");
			}

			if (ttl_seconds > 0)
			{
				if (!results.get<bool>(1))
				{
					return std::unexpected("failed to set TTL.");
				}
			}

			return {};
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to set value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::get(const std::string& key) -> std::expected<std::string, std::string>
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
				return std::unexpected(std::format("failed to get value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			auto result = redis->get(key);
			if (!result.has_value())
			{
				return std::unexpected(std::format("failed to get value {}", key));
			}

			return result.value();
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to get value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::lpush(const std::string& key, const std::vector<std::string>& values, long ttl_seconds) -> std::expected<long long, std::string>
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
				return std::unexpected(std::format("failed to lpush value: {}", connect_result.error()));
			}
		}

		auto transaction = connector_->get_transaction();
		if (transaction == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			transaction->lpush(key, values.begin(), values.end());
			if (ttl_seconds > 0)
			{
				transaction->expire(key, ttl_seconds);
			}
			auto results = transaction->exec();

			auto list_length = results.get<long long>(0);
			if (list_length < 0)
			{
				return std::unexpected(std::format("failed to lpush value {}", key));
			}

			if (ttl_seconds > 0)
			{
				if (!results.get<bool>(1))
				{
					return std::unexpected(std::format("failed to set TTL for {}", key));
				}
			}

			return list_length;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to lpush value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::rpush(const std::string& key, const std::vector<std::string>& values, long ttl_seconds) -> std::expected<long long, std::string>
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
				return std::unexpected(std::format("failed to rpush value: {}", connect_result.error()));
			}
		}

		auto transaction = connector_->get_transaction();
		if (transaction == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			transaction->rpush(key, values.begin(), values.end());
			if (ttl_seconds > 0)
			{
				transaction->expire(key, ttl_seconds);
			}
			auto results = transaction->exec();

			auto list_length = results.get<long long>(0);
			if (list_length < 0)
			{
				return std::unexpected(std::format("failed to rpush value {}", key));
			}

			if (ttl_seconds > 0)
			{
				if (!results.get<bool>(1))
				{
					return std::unexpected(std::format("failed to set TTL for {}", key));
				}
			}

			return list_length;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to rpush value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::lpop(const std::string& key) -> std::expected<std::optional<std::string>, std::string>
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
				return std::unexpected(std::format("failed to lpop value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		std::optional<std::string> value;
		try
		{
			value = redis->lpop(key);
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to lpop value {}: {}", key, err.what()));
		}

		return value;
	}

	auto RedisClient::rpop(const std::string& key) -> std::expected<std::optional<std::string>, std::string>
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
				return std::unexpected(std::format("failed to rpop value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			auto value = redis->rpop(key);
			if (!value.has_value())
			{
				return std::unexpected(std::format("failed to rpop value {}", key));
			}

			return value;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to rpop value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::lrange(const std::string& key, long start, long stop) -> std::expected<std::vector<std::string>, std::string>
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
				return std::unexpected(std::format("failed to lrange value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			std::vector<std::string> values;
			redis->lrange(key, start, stop, std::back_inserter(values));

			return values;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to lrange value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::blpop(const std::string& key, const std::optional<long>& timeout_seconds) -> std::expected<std::optional<std::string>, std::string>
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
				return std::unexpected(std::format("failed to blpop value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			auto result = redis->blpop(key, timeout_seconds.value_or(0));
			if (result)
			{
				return std::optional<std::string>(result->second);
			}

			return std::unexpected(std::format("failed to blpop value {}", key));
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to blpop value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::zadd(const std::string& key,
						   const std::vector<std::pair<std::string, double>>& members,
						   long ttl_seconds) -> std::expected<long long, std::string>
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
				return std::unexpected(std::format("failed to zadd value: {}", connect_result.error()));
			}
		}

		auto transaction = connector_->get_transaction();
		if (transaction == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			transaction->zadd(key, members.begin(), members.end());
			if (ttl_seconds > 0)
			{
				transaction->expire(key, ttl_seconds);
			}
			auto results = transaction->exec();

			auto added = results.get<long long>(0);
			if (added < 0)
			{
				return std::unexpected(std::format("failed to zadd value {}", key));
			}

			if (ttl_seconds > 0)
			{
				if (!results.get<bool>(1))
				{
					return std::unexpected(std::format("failed to set TTL for {}", key));
				}
			}

			return added;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to zadd value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::zrange(const std::string& key, long start, long stop, bool with_scores) -> std::expected<std::vector<std::string>, std::string>
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
				return std::unexpected(std::format("failed to zrange value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			if (!with_scores)
			{
				std::vector<std::string> values;
				redis->zrange(key, start, stop, std::back_inserter(values));

				return values;
			}

			std::vector<std::string> values;
			std::vector<std::pair<std::string, double>> tmp;
			redis->zrange(key, start, stop, std::back_inserter(tmp));
			for (const auto& pair : tmp)
			{
				values.emplace_back(pair.first + ":" + std::to_string(pair.second));
			}

			return values;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to zrange value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::zrem(const std::string& key, const std::vector<std::string>& members) -> std::expected<long long, std::string>
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
				return std::unexpected(std::format("failed to zrem value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			auto removed = redis->zrem(key, members.begin(), members.end());
			if (removed < 0)
			{
				return std::unexpected(std::format("failed to zrem value {}", key));
			}

			return removed;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to zrem value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::set_ttl(const std::string& key, long ttl_seconds) -> std::expected<void, std::string>
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
				return std::unexpected(std::format("failed to expire value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			if (!redis->expire(key, ttl_seconds))
			{
				return std::unexpected("failed to set TTL.");
			}

			return {};
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to expire value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::llen(const std::string& key) -> std::expected<long long, std::string>
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
				return std::unexpected(std::format("failed to llen value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			auto length = redis->llen(key);
			if (length < 0)
			{
				return std::unexpected(std::format("failed to llen value {}", key));
			}

			return length;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to llen value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::del(const std::string& key) -> std::expected<long long, std::string>
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
				return std::unexpected(std::format("failed to del value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			auto deleted = redis->del(key);
			if (deleted < 0)
			{
				return std::unexpected(std::format("failed to del value {}", key));
			}

			return deleted;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to del value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::lrem(const std::string& key, long count, const std::string& value) -> std::expected<long long, std::string>
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
				return std::unexpected(std::format("failed to lrem value: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			auto removed = redis->lrem(key, count, value);
			if (removed < 0)
			{
				return std::unexpected(std::format("failed to lrem value {}", key));
			}

			return removed;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return std::unexpected(std::format("failed to lrem value {}: {}", key, err.what()));
		}
	}

	auto RedisClient::xadd(const std::string& key, const std::map<std::string, std::string>& fields, const std::optional<std::string>& id, long maxlen, long ttl_seconds)
		-> std::expected<std::string, std::string>
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
				return std::unexpected(std::format("failed to xadd: {}", connect_result.error()));
			}
		}

		auto transaction = connector_->get_transaction();
		if (transaction == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			std::vector<std::pair<sw::redis::StringView, sw::redis::StringView>> field_pairs;
			for (const auto& [key, value] : fields)
			{
				field_pairs.emplace_back(sw::redis::StringView(key), sw::redis::StringView(value));
			}

			std::string result_id;
			if (maxlen > 0)
			{
				if (id.has_value())
				{
					transaction->xadd(key, id.value(), field_pairs.begin(), field_pairs.end(), maxlen);
				}
				else
				{
					transaction->xadd(key, "*", field_pairs.begin(), field_pairs.end(), maxlen);
				}

				if (ttl_seconds > 0)
				{
					transaction->expire(key, ttl_seconds);
				}

				auto results = transaction->exec();

				if (ttl_seconds > 0 && !results.get<bool>(1))
				{
					return std::unexpected("failed to set TTL.");
				}

				return results.get<std::string>(0);
			}

			if (id.has_value())
			{
				transaction->xadd(key, id.value(), field_pairs.begin(), field_pairs.end());
			}
			else
			{
				transaction->xadd(key, "*", field_pairs.begin(), field_pairs.end());
			}

			if (ttl_seconds > 0)
			{
				transaction->expire(key, ttl_seconds);
			}

			auto results = transaction->exec();

			if (ttl_seconds > 0 && !results.get<bool>(1))
			{
				return std::unexpected("failed to set TTL.");
			}

			return results.get<std::string>(0);
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return std::unexpected(std::format("failed to xadd: {}", err.what()));
		}
	}

	auto RedisClient::xread(const std::vector<std::string>& keys, const std::vector<std::string>& ids, long count, long block)
		-> std::expected<std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>, std::string>
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
				return std::unexpected(std::format("failed to xread: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>> result;

			if (keys.size() != ids.size())
			{
				return std::unexpected("keys and ids size mismatch");
			}

			for (size_t i = 0; i < keys.size(); ++i)
			{
				const auto& key = keys[i];
				const auto& id = ids[i];

				if (count > 0)
				{
					if (block > 0)
					{
						redis->xread(key, id, std::chrono::milliseconds(block), count, std::back_inserter(result));
					}
					else
					{
						redis->xread(key, id, count, std::back_inserter(result));
					}

					continue;
				}

				if (block > 0)
				{
					redis->xread(key, id, std::chrono::milliseconds(block), std::back_inserter(result));
				}
				else
				{
					redis->xread(key, id, std::back_inserter(result));
				}
			}

			return result;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return std::unexpected(std::format("failed to xread: {}", err.what()));
		}
	}

	auto RedisClient::xlen(const std::string& key) -> std::expected<long long, std::string>
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
				return std::unexpected(std::format("failed to xlen: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			auto length = redis->xlen(key);
			return length;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return std::unexpected(std::format("failed to xlen: {}", err.what()));
		}
	}

	auto RedisClient::xdel(const std::string& key, const std::vector<std::string>& ids) -> std::expected<long long, std::string>
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
				return std::unexpected(std::format("failed to xdel: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			auto deleted = redis->xdel(key, ids.begin(), ids.end());
			return deleted;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return std::unexpected(std::format("failed to xdel: {}", err.what()));
		}
	}

	auto RedisClient::xrange(const std::string& key,
							 const std::string& start,
							 const std::string& end,
							 long count) -> std::expected<std::vector<std::pair<std::string, std::map<std::string, std::string>>>, std::string>
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
				return std::unexpected(std::format("failed to xrange: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			std::vector<std::pair<std::string, std::map<std::string, std::string>>> result;
			if (count > 0)
			{
				redis->xrange(key, start, end, count, std::back_inserter(result));

				return result;
			}

			redis->xrange(key, start, end, std::back_inserter(result));

			return result;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return std::unexpected(std::format("failed to xrange: {}", err.what()));
		}
	}

	auto RedisClient::xgroup_create(const std::string& key,
									const std::string& group_name,
									const std::string& id,
									bool mkstream) -> std::expected<void, std::string>
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
				return std::unexpected(std::format("failed to create group: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			if (mkstream)
			{
				redis->xgroup_create(key, group_name, id, true);

				return {};
			}

			redis->xgroup_create(key, group_name, id);

			return {};
		}
		catch (const sw::redis::Error& err)
		{
			auto error_message = std::string(err.what());
			if (error_message.find("BUSYGROUP") != std::string::npos)
			{
				return {};
			}

			connector_->disconnect();
			return std::unexpected(std::format("failed to create group: {}", error_message));
		}
	}

	auto RedisClient::xreadgroup(const std::string& group_name,
								 const std::string& consumer_name,
								 const std::vector<std::string>& keys,
								 const std::vector<std::string>& ids,
								 long count,
								 long block)
		-> std::expected<std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>, std::string>
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
				return std::unexpected(std::format("failed to xreadgroup: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>> result;

			if (keys.size() != ids.size())
			{
				return std::unexpected("keys and ids size mismatch");
			}

			for (size_t i = 0; i < keys.size(); ++i)
			{
				const auto& key = keys[i];
				const auto& id = ids[i];

				if (count > 0)
				{
					if (block > 0)
					{
						redis->xreadgroup(group_name, consumer_name, key, id, std::chrono::milliseconds(block), count, std::back_inserter(result));
					}
					else
					{
						redis->xreadgroup(group_name, consumer_name, key, id, count, std::back_inserter(result));
					}

					continue;
				}

				if (block > 0)
				{
					redis->xreadgroup(group_name, consumer_name, key, id, std::chrono::milliseconds(block), std::back_inserter(result));
				}
				else
				{
					redis->xreadgroup(group_name, consumer_name, key, id, std::back_inserter(result));
				}
			}

			return result;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return std::unexpected(std::format("failed to xreadgroup: {}", err.what()));
		}
	}

	auto RedisClient::xack(const std::string& key,
						   const std::string& group_name,
						   const std::vector<std::string>& ids) -> std::expected<long long, std::string>
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
				return std::unexpected(std::format("failed to xack: {}", connect_result.error()));
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return std::unexpected("failed to get redis connection.");
		}

		try
		{
			auto acknowledged = redis->xack(key, group_name, ids.begin(), ids.end());
			return acknowledged;
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return std::unexpected(std::format("failed to xack: {}", err.what()));
		}
	}
}
