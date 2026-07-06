#pragma once

#include "AWSSQSBase.h"

#include "aws/core/utils/memory/stl/AWSString.h"
#include <aws/core/Aws.h>
#include <aws/core/Region.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/sqs/SQSClient.h>

#include <expected>
#include <string>

namespace AWSService
{

	class AWSSQSPublisher : public AWSSQSBase
	{
	public:
		AWSSQSPublisher(const Aws::Client::ClientConfiguration& config = {});
		AWSSQSPublisher(const std::string& access_key_id, const std::string& secret_key, const std::string& target_region = Aws::Region::US_EAST_1);
		AWSSQSPublisher(const std::string& access_key_id, const std::string& secret_key, const Aws::Client::ClientConfiguration& config);
		~AWSSQSPublisher();

		// Send to an explicitly named queue.
		auto send_message_to(const Aws::String& queue_url,
							 const Aws::String& message_body,
							 Aws::String message_group_id,
							 Aws::String message_deduplication_id = "",
							 int delay_seconds = 0) -> std::expected<void, std::string>;
		// Send to the queue configured via sqs_url().
		auto send_message(const Aws::String& message_body, Aws::String message_group_id, Aws::String message_deduplication_id = "", int delay_seconds = 0)
			-> std::expected<void, std::string>;
	};
} // namespace AWSService
