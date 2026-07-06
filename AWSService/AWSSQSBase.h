#pragma once

#include <Job.h>
#include <JobPriorities.h>
#include <ThreadPool.h>

#include "aws/core/utils/memory/stl/AWSString.h"
#include <aws/core/Aws.h>
#include <aws/core/Region.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/sqs/model/ChangeMessageVisibilityRequest.h>
#include <aws/sqs/model/DeleteMessageRequest.h>
#include <aws/sqs/model/ReceiveMessageRequest.h>
#include <aws/sqs/model/SendMessageRequest.h>

#include <aws/sqs/SQSClient.h>

#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>

namespace AWSService
{

	class AWSSQSBase
	{
	public:
		AWSSQSBase(const Aws::Client::ClientConfiguration& config = {});
		AWSSQSBase(const std::string& access_key_id, const std::string& secret_key, const std::string& target_region = Aws::Region::US_EAST_1);
		AWSSQSBase(const std::string& access_key_id, const std::string& secret_key, const Aws::Client::ClientConfiguration& config);
		~AWSSQSBase();

		auto start() -> std::expected<void, std::string>;
		auto wait_stop() -> std::expected<void, std::string>;
		auto stop() -> void;

		auto sqs_url() -> std::string;
		auto sqs_url(const std::string& sqs_url) -> void;

	protected:
		auto basic_send_message(const Aws::String& queue_url,
								const Aws::String& message_body,
								Aws::String message_group_id,
								Aws::String message_deduplication_id = "",
								int delay_seconds = 0) -> std::expected<void, std::string>;
		auto basic_receive_message(const Aws::String& queue_url, int wait_time_seconds, int visibility_timeout, int max_number_of_messages)
			-> std::expected<void, std::string>;
		auto basic_stop_message() -> std::expected<void, std::string>;

		// Perform a single receive + process + delete cycle.
		// Returns: true  -> no messages were available (caller should back off)
		//          false -> at least one message was received and processed
		//          unexpected -> error string
		auto receive_process_once(const Aws::String& queue_url, int wait_time_seconds, int visibility_timeout, int max_number_of_messages)
			-> std::expected<bool, std::string>;

		auto create_thread_pool() -> std::expected<void, std::string>;
		auto destroy_thread_pool() -> std::expected<void, std::string>;

		Aws::SQS::SQSClient sqs_client_;
		Aws::Auth::AWSCredentials credentials_;
		Aws::Client::ClientConfiguration client_config_;

		std::unique_ptr<Thread::ThreadPool> thread_pool_;
		std::string sqs_url_;

		std::promise<void> stop_promise_;
		std::optional<std::future<void>> stop_future_;

		std::function<std::expected<void, std::string>(const std::string&)> handler_;
	};
} // namespace AWSService
