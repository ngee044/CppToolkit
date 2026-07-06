#include "AWSSQSPublisher.h"

namespace AWSService
{

	AWSSQSPublisher::AWSSQSPublisher(const Aws::Client::ClientConfiguration& config) : AWSSQSBase(config) {}

	AWSSQSPublisher::AWSSQSPublisher(const std::string& access_key_id, const std::string& secret_key, const std::string& target_region)
		: AWSSQSBase(access_key_id, secret_key, target_region)
	{
	}

	AWSSQSPublisher::AWSSQSPublisher(const std::string& access_key_id, const std::string& secret_key, const Aws::Client::ClientConfiguration& config)
		: AWSSQSBase(access_key_id, secret_key, config)
	{
	}

	AWSSQSPublisher::~AWSSQSPublisher() {}

	auto AWSSQSPublisher::send_message_to(const Aws::String& queue_url,
										  const Aws::String& message_body,
										  Aws::String message_group_id,
										  Aws::String message_deduplication_id,
										  int delay_seconds) -> std::expected<void, std::string>
	{
		return basic_send_message(queue_url, message_body, message_group_id, message_deduplication_id, delay_seconds);
	}

	auto AWSSQSPublisher::send_message(const Aws::String& message_body, Aws::String message_group_id, Aws::String message_deduplication_id, int delay_seconds)
		-> std::expected<void, std::string>
	{
		return send_message_to(Aws::String(sqs_url().c_str()), message_body, message_group_id, message_deduplication_id, delay_seconds);
	}

} // namespace AWSService
