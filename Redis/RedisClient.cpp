// RedisClient.cpp
#include "RedisClient.h"

#include <iostream>

namespace Redis
{
	RedisClient::RedisClient(const std::string& address, const int& port, const TLSOptions& tls_options, const int& db_index)
		: connector_(std::make_shared<RedisConnector>(address, port, tls_options, db_index))
	{
	}

	RedisClient::~RedisClient() { connector_.reset(); }

	auto RedisClient::connect() -> std::tuple<bool, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { false, "Connector is not created." };
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

	auto RedisClient::disconnect() -> std::tuple<bool, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { false, "Connector is not created." };
		}

		return connector_->disconnect();
	}

	auto RedisClient::set(const std::string& key, const std::string& value, long ttl_seconds) -> std::tuple<bool, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { false, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { false, fmt::format("failed to set value: {}", connect_error.value()) };
			}
		}

		auto transaction = connector_->get_transaction();
		if (transaction == nullptr)
		{
			return { false, "failed to get redis connection." };
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
				return { false, "failed to set value." };
			}

			if (ttl_seconds > 0)
			{
				if (!results.get<bool>(1))
				{
					return { false, "failed to set TTL." };
				}
			}

			return { true, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { false, fmt::format("failed to set value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::get(const std::string& key) -> std::tuple<std::string, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { "", "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { "", fmt::format("failed to get value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { "", "failed to get redis connection." };
		}

		try
		{
			auto result = redis->get(key);
			if (!result.has_value())
			{
				return { "", fmt::format("failed to get value {}", key) };
			}

			return { result.value(), std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { "", fmt::format("failed to get value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::lpush(const std::string& key, const std::vector<std::string>& values, long ttl_seconds) -> std::tuple<long long, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { 0, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { 0, fmt::format("failed to lpush value: {}", connect_error.value()) };
			}
		}

		auto transaction = connector_->get_transaction();
		if (transaction == nullptr)
		{
			return { false, "failed to get redis connection." };
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
				return { 0, fmt::format("failed to lpush value {}", key) };
			}

			if (ttl_seconds > 0)
			{
				if (!results.get<bool>(1))
				{
					return { 0, fmt::format("failed to set TTL for {}", key) };
				}
			}

			return { list_length, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { 0, fmt::format("failed to lpush value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::rpush(const std::string& key, const std::vector<std::string>& values, long ttl_seconds) -> std::tuple<long long, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { 0, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { 0, fmt::format("failed to rpush value: {}", connect_error.value()) };
			}
		}

		auto transaction = connector_->get_transaction();
		if (transaction == nullptr)
		{
			return { false, "failed to get redis connection." };
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
				return { 0, fmt::format("failed to rpush value {}", key) };
			}

			if (ttl_seconds > 0)
			{
				if (!results.get<bool>(1))
				{
					return { 0, fmt::format("failed to set TTL for {}", key) };
				}
			}

			return { list_length, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { 0, fmt::format("failed to rpush value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::lpop(const std::string& key) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { std::nullopt, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { std::nullopt, fmt::format("failed to lpop value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { std::nullopt, "failed to get redis connection." };
		}

		std::optional<std::string> value;
		try
		{
			value = redis->lpop(key);
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { std::nullopt, fmt::format("failed to lpop value {}: {}", key, err.what()) };
		}

		return { value, std::nullopt };
	}

	auto RedisClient::rpop(const std::string& key) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { std::nullopt, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { std::nullopt, fmt::format("failed to rpop value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { std::nullopt, "failed to get redis connection." };
		}

		try
		{
			auto value = redis->rpop(key);
			if (!value.has_value())
			{
				return { std::nullopt, fmt::format("failed to rpop value {}", key) };
			}

			return { value, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { std::nullopt, fmt::format("failed to rpop value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::lrange(const std::string& key, long start, long stop) -> std::tuple<std::vector<std::string>, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { std::vector<std::string>{}, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { std::vector<std::string>{}, fmt::format("failed to lrange value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { std::vector<std::string>{}, "failed to get redis connection." };
		}

		try
		{
			std::vector<std::string> values;
			redis->lrange(key, start, stop, std::back_inserter(values));

			return { values, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { std::vector<std::string>{}, fmt::format("failed to lrange value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::blpop(const std::string& key, const std::optional<long>& timeout_seconds) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { std::nullopt, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { std::nullopt, fmt::format("failed to blpop value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { std::nullopt, "failed to get redis connection." };
		}

		try
		{
			auto result = redis->blpop(key, timeout_seconds.value_or(0));
			if (result)
			{
				return { result->second, std::nullopt };
			}

			return { std::nullopt, fmt::format("failed to blpop value {}", key) };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { std::nullopt, fmt::format("failed to blpop value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::zadd(const std::string& key,
						   const std::vector<std::pair<std::string, double>>& members,
						   long ttl_seconds) -> std::tuple<long long, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { 0, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { 0, fmt::format("failed to zadd value: {}", connect_error.value()) };
			}
		}

		auto transaction = connector_->get_transaction();
		if (transaction == nullptr)
		{
			return { false, "failed to get redis connection." };
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
				return { 0, fmt::format("failed to zadd value {}", key) };
			}

			if (ttl_seconds > 0)
			{
				if (!results.get<bool>(1))
				{
					return { 0, fmt::format("failed to set TTL for {}", key) };
				}
			}

			return { added, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { 0, fmt::format("failed to zadd value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::zrange(const std::string& key, long start, long stop, bool with_scores) -> std::tuple<std::vector<std::string>, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { std::vector<std::string>{}, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { std::vector<std::string>{}, fmt::format("failed to zrange value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { std::vector<std::string>{}, "failed to get redis connection." };
		}

		try
		{
			if (!with_scores)
			{
				std::vector<std::string> values;
				redis->zrange(key, start, stop, std::back_inserter(values));

				return { values, std::nullopt };
			}

			std::vector<std::string> values;
			std::vector<std::pair<std::string, double>> tmp;
			redis->zrange(key, start, stop, std::back_inserter(tmp));
			for (const auto& pair : tmp)
			{
				values.emplace_back(pair.first + ":" + std::to_string(pair.second));
			}

			return { values, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { std::vector<std::string>{}, fmt::format("failed to zrange value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::zrem(const std::string& key, const std::vector<std::string>& members) -> std::tuple<long long, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { 0, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { 0, fmt::format("failed to zrem value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { false, "failed to get redis connection." };
		}

		try
		{
			auto removed = redis->zrem(key, members.begin(), members.end());
			if (removed < 0)
			{
				return { 0, fmt::format("failed to zrem value {}", key) };
			}

			return { removed, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { 0, fmt::format("failed to zrem value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::set_ttl(const std::string& key, long ttl_seconds) -> std::tuple<bool, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { false, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { false, fmt::format("failed to expire value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { false, "failed to get redis connection." };
		}

		try
		{
			if (!redis->expire(key, ttl_seconds))
			{
				return { false, "failed to set TTL." };
			}

			return { true, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { false, fmt::format("failed to expire value {}: {}", key, err.what()) };
		}

		return { true, std::nullopt };
	}

	auto RedisClient::llen(const std::string& key) -> std::tuple<long long, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { 0, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { 0, fmt::format("failed to llen value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { false, "failed to get redis connection." };
		}

		try
		{
			auto length = redis->llen(key);
			if (length < 0)
			{
				return { 0, fmt::format("failed to llen value {}", key) };
			}

			return { length, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { 0, fmt::format("failed to llen value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::del(const std::string& key) -> std::tuple<long long, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { 0, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { 0, fmt::format("failed to del value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { false, "failed to get redis connection." };
		}

		try
		{
			auto deleted = redis->del(key);
			if (deleted < 0)
			{
				return { 0, fmt::format("failed to del value {}", key) };
			}

			return { deleted, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { 0, fmt::format("failed to del value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::lrem(const std::string& key, long count, const std::string& value) -> std::tuple<long long, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { 0, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { 0, fmt::format("failed to lrem value: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { false, "failed to get redis connection." };
		}

		try
		{
			auto removed = redis->lrem(key, count, value);
			if (removed < 0)
			{
				return { 0, fmt::format("failed to lrem value {}", key) };
			}

			return { removed, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();

			return { 0, fmt::format("failed to lrem value {}: {}", key, err.what()) };
		}
	}

	auto RedisClient::xadd(const std::string& key, const std::map<std::string, std::string>& fields, const std::optional<std::string>& id, long maxlen, long ttl_seconds)
		-> std::tuple<std::string, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { "", "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { "", fmt::format("failed to xadd: {}", connect_error.value()) };
			}
		}

		auto transaction = connector_->get_transaction();
		if (transaction == nullptr)
		{
			return { "", "failed to get redis connection." };
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
					return { "", "failed to set TTL." };
				}

				return { results.get<std::string>(0), std::nullopt };
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
				return { "", "failed to set TTL." };
			}

			return { results.get<std::string>(0), std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return { "", fmt::format("failed to xadd: {}", err.what()) };
		}
	}

	auto RedisClient::xread(const std::vector<std::string>& keys, const std::vector<std::string>& ids, long count, long block)
		-> std::tuple<std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>{}, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>{},
						 fmt::format("failed to xread: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>{}, "failed to get redis connection." };
		}

		try
		{
			std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>> result;

			if (keys.size() != ids.size())
			{
				return { std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>{}, "keys and ids size mismatch" };
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

			return { result, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return { std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>{},
					 fmt::format("failed to xread: {}", err.what()) };
		}
	}

	auto RedisClient::xlen(const std::string& key) -> std::tuple<long long, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { 0, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { 0, fmt::format("failed to xlen: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { 0, "failed to get redis connection." };
		}

		try
		{
			auto length = redis->xlen(key);
			return { length, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return { 0, fmt::format("failed to xlen: {}", err.what()) };
		}
	}

	auto RedisClient::xdel(const std::string& key, const std::vector<std::string>& ids) -> std::tuple<long long, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { 0, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { 0, fmt::format("failed to xdel: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { 0, "failed to get redis connection." };
		}

		try
		{
			auto deleted = redis->xdel(key, ids.begin(), ids.end());
			return { deleted, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return { 0, fmt::format("failed to xdel: {}", err.what()) };
		}
	}

	auto RedisClient::xrange(const std::string& key,
							 const std::string& start,
							 const std::string& end,
							 long count) -> std::tuple<std::vector<std::pair<std::string, std::map<std::string, std::string>>>, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { std::vector<std::pair<std::string, std::map<std::string, std::string>>>{}, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { std::vector<std::pair<std::string, std::map<std::string, std::string>>>{}, fmt::format("failed to xrange: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { std::vector<std::pair<std::string, std::map<std::string, std::string>>>{}, "failed to get redis connection." };
		}

		try
		{
			std::vector<std::pair<std::string, std::map<std::string, std::string>>> result;
			if (count > 0)
			{
				redis->xrange(key, start, end, count, std::back_inserter(result));

				return { result, std::nullopt };
			}

			redis->xrange(key, start, end, std::back_inserter(result));

			return { result, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return { std::vector<std::pair<std::string, std::map<std::string, std::string>>>{}, fmt::format("failed to xrange: {}", err.what()) };
		}
	}

	auto RedisClient::xgroup_create(const std::string& key,
									const std::string& group_name,
									const std::string& id,
									bool mkstream) -> std::tuple<bool, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { false, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { false, fmt::format("failed to create group: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { false, "failed to get redis connection." };
		}

		try
		{
			if (mkstream)
			{
				redis->xgroup_create(key, group_name, id, true);

				return { true, std::nullopt };
			}

			redis->xgroup_create(key, group_name, id);

			return { true, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			auto error_message = std::string(err.what());
			if (error_message.find("BUSYGROUP") != std::string::npos)
			{
				return { true, std::nullopt };
			}

			connector_->disconnect();
			return { false, fmt::format("failed to create group: {}", error_message) };
		}
	}

	auto RedisClient::xreadgroup(const std::string& group_name,
								 const std::string& consumer_name,
								 const std::vector<std::string>& keys,
								 const std::vector<std::string>& ids,
								 long count,
								 long block)
		-> std::tuple<std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>{}, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>{},
						 fmt::format("failed to xreadgroup: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>{}, "failed to get redis connection." };
		}

		try
		{
			std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>> result;

			if (keys.size() != ids.size())
			{
				return { std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>{}, "keys and ids size mismatch" };
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

			return { result, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return { std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::map<std::string, std::string>>>>>{},
					 fmt::format("failed to xreadgroup: {}", err.what()) };
		}
	}

	auto RedisClient::xack(const std::string& key,
						   const std::string& group_name,
						   const std::vector<std::string>& ids) -> std::tuple<long long, std::optional<std::string>>
	{
		if (connector_ == nullptr)
		{
			return { 0, "Connector is not created." };
		}

		if (!connector_->is_connected())
		{
			auto [connected, connect_error] = connector_->connect();
			if (connect_error.has_value())
			{
				return { 0, fmt::format("failed to xack: {}", connect_error.value()) };
			}
		}

		auto redis = connector_->get_redis();
		if (redis == nullptr)
		{
			return { 0, "failed to get redis connection." };
		}

		try
		{
			auto acknowledged = redis->xack(key, group_name, ids.begin(), ids.end());
			return { acknowledged, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			connector_->disconnect();
			return { 0, fmt::format("failed to xack: {}", err.what()) };
		}
	}
}