#include "RedisClient.h"


namespace Redis
{
	RedisClient::RedisClient(const std::string& host, const int& port, const TLSOptions& tls_options, const int& db_index)
		: redis_connector_(std::make_shared<RedisConnector>(host, port, tls_options, db_index))
	{
	}

	RedisClient::~RedisClient()
	{
		redis_connector_->disconnect();
	}

	auto RedisClient::connect() -> std::tuple<bool, std::optional<std::string>>
	{
		if (redis_connector_ == nullptr)
		{
			return { false, "RedisConnector is null" };
		}

		return redis_connector_->connect();
	}

	auto RedisClient::is_connected() const -> bool
	{
		if (redis_connector_ == nullptr)
		{
			return false;
		}

		return redis_connector_->is_connected();
	}

	auto RedisClient::disconnect() -> std::tuple<bool, std::optional<std::string>>
	{
		if (redis_connector_ == nullptr)
		{
			return { false, "RedisConnector is null" };
		}

		return redis_connector_->disconnect();
	}

	auto RedisClient::set(const std::string& key, const std::string& value, std::uint32_t ttl_sec) -> std::tuple<bool, std::optional<std::string>>
	{
		if (redis_connector_ == nullptr)
		{
			return { false, "RedisConnector is null" };
		}

		if (!redis_connector_->is_connected())
		{
			auto [connected, connect_error] = redis_connector_->connect();
			if (!connected)
			{
				return { false, connect_error };
			}

			auto transaction = redis_connector_->get_transaction();
			if (transaction == nullptr)
			{
				return { false, "Transaction is null" };
			}

			try
			{
				transaction->set(key, value);
				if (ttl_sec > 0)
				{
					transaction->expire(key, ttl_sec);
				}
				auto results = transaction->exec();

				if (!results.get<bool>(0))
				{
					return { false, "Failed to set key" };
				}

				if (ttl_sec > 0)
				{
					if (!results.get<bool>(1))
					{
						return { false, "Failed to set ttl" };
					}
				}

				return { true, std::nullopt };
			}
			catch (const sw::redis::Error& err)
			{
				redis_connector_->disconnect();
				
				return { false, fmt::format("failed to set value {}: {}", key, err.what()) };
			}

		}
	}

	auto RedisClient::get(const std::string& key) -> std::tuple<std::string, std::optional<std::string>>
	{
		if (redis_connector_ == nullptr)
		{
			return { "", "RedisConnector is null" };
		}

		if (!redis_connector_->is_connected())
		{
			auto [connected, connect_error] = redis_connector_->connect();
			if (!connected)
			{
				return { "", fmt::format("failed to get value: {}", connect_error.value()) };
			}

			auto redis = redis_connector_->get_redis();
			if (redis == nullptr)
			{
				return { "", "failed to get redis connection." };
			}

			try
			{
				auto result = redis->get(key);
				if (!result.has_value())
				{
					return { "", fmt::format("failed to get value: {}", key) };
				}

				return { result.value(), std::nullopt };
			}
			catch (const sw::redis::Error& err)
			{
				redis_connector_->disconnect();

				return { "", fmt::format("failed to get value {}: {}", key, err.what()) };
			}
		}

	}

	auto RedisClient::set_ttl(const std::string& key, std::uint32_t ttl_sec) -> std::tuple<bool, std::optional<std::string>>
	{
		if (redis_connector_ == nullptr)
		{
			return { false, "RedisConnector is null" };
		}

		if (!redis_connector_->is_connected())
		{
			auto [connected, connect_error] = redis_connector_->connect();
			if (!connected)
			{
				return { false, fmt::format("failed to expire value: {}", connect_error.value()) };
			}
		}

		auto redis = redis_connector_->get_redis();
		if (redis == nullptr)
		{
			return { false, "failed to get redis connection." };
		}

		try
		{
			if (!redis->expire(key, ttl_sec))
			{
				return { false, fmt::format("failed to expire value: {}", key) };
			}
		}
		catch(const sw::redis::Error& err)
		{
			redis_connector_->disconnect();

			return { false, fmt::format("failed to expire value {}: {}", key, err.what()) };
		}
		
		return { true, std::nullopt };
	}
}