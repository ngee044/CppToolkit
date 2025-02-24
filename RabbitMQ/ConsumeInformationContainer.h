#pragma once

#include "ConsumeInformation.h"

#include <map>
#include <string>

namespace RabbitMQ
{
	class ConsumeInformationContainer
	{
	public:
		ConsumeInformationContainer(const int& heartbeat);
		ConsumeInformationContainer(const int& heartbeat, const std::vector<ConsumeInformation>& consume_informations);

		auto exists_consume_information(const std::string& queue_name) const -> bool;
		auto add_consume_information(const ConsumeInformation& information) -> std::tuple<bool, std::optional<std::string>>;
		auto remove_consume_information(const std::string& queue_name) -> std::tuple<std::optional<ConsumeInformation>, std::optional<std::string>>;

		auto get_heartbeat() const -> int;
		auto get_consume_informations() const -> std::vector<ConsumeInformation>;
		auto get_consume_callback(const std::string& queue_name) const
			-> std::optional<std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)>>;

	private:
		int heartbeat_;
		std::map<std::string, ConsumeInformation> consume_informations_;
	};
}