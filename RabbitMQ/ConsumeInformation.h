#pragma once

#include <string>
#include <optional>
#include <expected>
#include <functional>

namespace RabbitMQ
{
	class ConsumeInformation
	{
	public:
		ConsumeInformation(const int& channel_id,
						   const std::string& queue_name,
						   const std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)>& callback);

		auto get_channel_id() const -> int;
		auto get_queue_name() const -> const std::string&;
		auto get_callback() const -> const std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)>&;

		auto get_consumer_tag() const -> const std::string&;
		auto set_consumer_tag(const std::string& consumer_tag) -> void;

	private:
		int channel_id_;
		std::string queue_name_;
		std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)> callback_;
		std::string consumer_tag_;
	};
}