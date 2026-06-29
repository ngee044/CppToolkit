#include "ConsumeInformation.h"

namespace RabbitMQ
{
	ConsumeInformation::ConsumeInformation(
		const int& channel_id,
		const std::string& queue_name,
		const std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)>& callback)
		: channel_id_(channel_id), queue_name_(queue_name), callback_(callback)
	{
	}

	auto ConsumeInformation::get_channel_id() const -> int { return channel_id_; }

	auto ConsumeInformation::get_queue_name() const -> const std::string& { return queue_name_; }

	auto ConsumeInformation::get_callback() const
		-> const std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)>&
	{
		return callback_;
	}

	auto ConsumeInformation::get_consumer_tag() const -> const std::string& { return consumer_tag_; }

	auto ConsumeInformation::set_consumer_tag(const std::string& consumer_tag) -> void { consumer_tag_ = consumer_tag; }
}