#pragma once

#include <string>
#include <optional>
#include <functional>

namespace RabbitMQ
{
	class ConsumeInformation
	{
	public:
		ConsumeInformation(const int& channel_id,
			const std::string& queue_name,
			const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)>& callback);

		auto get_channel_id() const -> int;
		auto get_queue_name() const -> const std::string&;
		auto get_callback() const -> const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)>&;

	private:
		int channel_id_;
		std::string queue_name_;
		std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)> callback_;
	};
}