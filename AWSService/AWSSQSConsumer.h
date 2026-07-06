#pragma once

#include "AWSSQSBase.h"

#include <aws/core/Aws.h>
#include <aws/core/Region.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/sqs/SQSClient.h>

#include <atomic>
#include <expected>
#include <functional>
#include <string>

namespace AWSService
{

	struct AWSSQSConsumerConfig
	{
		int wait_time_seconds = 30;
		int visibility_timeout = 300;
		int max_number_of_messages = 0;
	};

	class AWSSQSConsumer : public AWSSQSBase
	{
	public:
		AWSSQSConsumer(const Aws::Client::ClientConfiguration& config = {}, const AWSSQSConsumerConfig& consume_config = {});
		AWSSQSConsumer(const std::string& access_key_id,
					   const std::string& secret_key,
					   const std::string& target_region = Aws::Region::US_EAST_1,
					   const AWSSQSConsumerConfig& consume_config = {});
		AWSSQSConsumer(const std::string& access_key_id,
					   const std::string& secret_key,
					   const Aws::Client::ClientConfiguration& config,
					   const AWSSQSConsumerConfig& consume_config = {});
		~AWSSQSConsumer();

		auto register_consume_handler(std::function<std::expected<void, std::string>(const std::string&)> handler) -> std::expected<void, std::string>;
		auto start_consume() -> std::expected<void, std::string>;
		auto stop_consume() -> std::expected<void, std::string>;
		auto is_running() const -> bool;

	private:
		AWSSQSConsumerConfig consume_config_;
		std::atomic<bool> is_running_;
	};
} // namespace AWSService
