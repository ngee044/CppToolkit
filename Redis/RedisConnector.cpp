#include "RedisConnector.h"

#include "Logger.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include <sw/redis++/tls.h>

using namespace Utilities;

namespace Redis
{
	RedisConnector::RedisConnector(const std::string& host, int port, const TLSOptions& tlsOptions, const int& db_index)
		: host_(host), port_(port), tlsOptions_(tlsOptions), db_index_(db_index), redis_(nullptr)
	{
	}

	RedisConnector::~RedisConnector()
	{
		disconnect();
	}

	auto RedisConnector::connect() -> std::tuple<bool, std::optional<std::string>>
	{
		try
		{
			connection_options_.host = host_;
			connection_options_.port = port_;
			connection_options_.db = db_index_;

			connection_options_.tls.enabled = tlsOptions_.use_tls();
			connection_options_.tls.cacert = tlsOptions_.ca_cert();
			connection_options_.tls.cert = tlsOptions_.client_cert();
			connection_options_.tls.key = tlsOptions_.client_key();
			connection_options_.tls.verify_mode = tlsOptions_.verify_peer() ? REDIS_SSL_VERIFY_PEER : REDIS_SSL_VERIFY_NONE;

			redis_ = std::make_shared<sw::redis::Redis>(connection_options_);

			return { true, std::nullopt };
		}
		catch (const sw::redis::Error& err)
		{
			redis_.reset();

			return { false, fmt::format("cannot connect: {}", err.what()) };
		}
	}

	auto RedisConnector::disconnect() -> std::tuple<bool, std::optional<std::string>>
	{
		try
		{
			if (redis_ == nullptr)
			{
				return { true, std::nullopt };
			}

			redis_->subscriber().unsubscribe();
			redis_.reset();

			return { true, std::nullopt };
		}
		catch(const std::exception& err)
		{
			redis_.reset();

			return { false, fmt::format("cannot disconnect: {}", err.what()) };
		}
		
	}

	auto RedisConnector::is_connected() const -> bool
	{
		return redis_ != nullptr;
	}

	auto RedisConnector::get_redis() const -> std::shared_ptr<sw::redis::Redis>
	{
		return redis_;
	}

	auto RedisConnector::get_transaction() const -> std::shared_ptr<sw::redis::Transaction>
	{
		if (redis_ == nullptr)
		{
			return nullptr;
		}

		return std::make_shared<sw::redis::Transaction>(redis_->transaction(false, false));
	}

}