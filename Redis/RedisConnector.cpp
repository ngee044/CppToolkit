#include "RedisConnector.h"

#include "Job.h"
#include "Logger.h"
#include "Converter.h"
#include "ThreadWorker.h"

#include <format>

#include <sw/redis++/tls.h>

using namespace Utilities;

namespace Redis
{
	RedisConnector::RedisConnector(const std::string& address, const int& port, const TLSOptions& tls_options, const int& db_index)
		: address_(address), port_(port), tls_options_(tls_options), db_index_(db_index)
	{
	}

	RedisConnector::~RedisConnector() { disconnect(); }

	auto RedisConnector::connect(void) -> std::expected<void, std::string>
	{
		try
		{
			connection_options_.host = address_;
			connection_options_.port = port_;
			connection_options_.db = db_index_;

			connection_options_.tls.enabled = tls_options_.use_tls();
			connection_options_.tls.cacert = tls_options_.ca_cert();
			connection_options_.tls.cert = tls_options_.client_cert();
			connection_options_.tls.key = tls_options_.client_key();
			connection_options_.tls.verify_mode = tls_options_.verify_peer() ? REDIS_SSL_VERIFY_PEER : REDIS_SSL_VERIFY_NONE;

			redis_ = std::make_shared<sw::redis::Redis>(connection_options_);

			return {};
		}
		catch (const sw::redis::Error& err)
		{
			redis_.reset();

			return std::unexpected(std::format("cannot connect: {}", err.what()));
		}
	}

	auto RedisConnector::disconnect(void) -> std::expected<void, std::string>
	{
		try
		{
			if (redis_ == nullptr)
			{
				return {};
			}

			redis_.reset();

			return {};
		}
		catch (const std::exception& err)
		{
			redis_.reset();

			return std::unexpected(std::format("cannot disconnect: {}", err.what()));
		}
	}

	auto RedisConnector::is_connected(void) const -> bool { return redis_ != nullptr; }

	auto RedisConnector::get_redis(void) const -> std::shared_ptr<sw::redis::Redis> { return redis_; }

	auto RedisConnector::get_transaction(void) const -> std::shared_ptr<sw::redis::Transaction>
	{
		if (redis_ == nullptr)
		{
			return nullptr;
		}

		return std::make_shared<sw::redis::Transaction>(redis_->transaction(false, false));
	}
}
