#include "ConsumeInformationContainer.h"

#include "fmt/format.h"

namespace RabbitMQ
{
	ConsumeInformationContainer::ConsumeInformationContainer(const int& heartbeat) : heartbeat_(heartbeat) {}

	ConsumeInformationContainer::ConsumeInformationContainer(const int& heartbeat, const std::vector<ConsumeInformation>& consume_informations) : heartbeat_(heartbeat)
	{
		for (const auto& information : consume_informations)
		{
			consume_informations_.insert({ information.get_queue_name(), information });
		}
	}

	auto ConsumeInformationContainer::exists_consume_information(const std::string& queue_name) const -> bool
	{
		auto iter = consume_informations_.find(queue_name);
		if (iter == consume_informations_.end())
		{
			return false;
		}

		return true;
	}

	auto ConsumeInformationContainer::add_consume_information(const ConsumeInformation& information) -> std::tuple<bool, std::optional<std::string>>
	{
		auto iter = consume_informations_.find(information.get_queue_name());
		if (iter != consume_informations_.end())
		{
			return { false, fmt::format("Consume information for queue '{}' already exists", information.get_queue_name()) };
		}

		consume_informations_.insert({ information.get_queue_name(), information });

		return { true, std::nullopt };
	}

	auto ConsumeInformationContainer::remove_consume_information(const std::string& queue_name)
		-> std::tuple<std::optional<ConsumeInformation>, std::optional<std::string>>
	{
		auto iter = consume_informations_.find(queue_name);
		if (iter == consume_informations_.end())
		{
			return { std::nullopt, fmt::format("Consume information for queue '{}' does not exist", queue_name) };
		}

		const auto information = iter->second;

		consume_informations_.erase(iter);

		return { information, std::nullopt };
	}

	auto ConsumeInformationContainer::get_heartbeat() const -> int { return heartbeat_; }

	auto ConsumeInformationContainer::get_consume_informations() const -> std::vector<ConsumeInformation>
	{
		std::vector<ConsumeInformation> result;
		for (const auto& [key, value] : consume_informations_)
		{
			result.push_back(value);
		}

		return result;
	}

	auto ConsumeInformationContainer::get_consume_callback(const std::string& queue_name) const
		-> std::optional<std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)>>
	{
		auto iter = consume_informations_.find(queue_name);
		if (iter == consume_informations_.end())
		{
			return std::nullopt;
		}

		return iter->second.get_callback();
	}

}