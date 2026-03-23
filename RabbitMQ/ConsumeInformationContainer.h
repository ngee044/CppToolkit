#pragma once

#include "ConsumeInformation.h"

#include <map>
#include <string>
#include <expected>

namespace RabbitMQ
{
	class ConsumeInformationContainer
	{
	public:
		ConsumeInformationContainer(const int& heartbeat);
		ConsumeInformationContainer(const int& heartbeat, const std::vector<ConsumeInformation>& consume_informations);

		auto exists_consume_information(const std::string& queue_name) const -> bool;
		auto add_consume_information(const ConsumeInformation& information) -> std::expected<void, std::string>;
		auto remove_consume_information(const std::string& queue_name) -> std::expected<ConsumeInformation, std::string>;

		auto get_heartbeat() const -> int;
		auto get_consume_informations() const -> std::vector<ConsumeInformation>;
		auto get_consume_callback(const std::string& queue_name) const
			-> std::optional<std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)>>;

	private:
		int heartbeat_;
		std::map<std::string, ConsumeInformation> consume_informations_;
	};
}