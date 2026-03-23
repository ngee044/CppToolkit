#pragma once

#include "RabbitMQBase.h"

#include <functional>
#include <string>

namespace RabbitMQ
{
	class WorkQueueConsume : public RabbitMQBase
	{
	public:
		WorkQueueConsume(const std::string& host, int port, const std::string& user_name, const std::string& password, const SSLOptions& ssl_options = SSLOptions());

		auto connect(int heartbeat) -> std::expected<void, std::string>;
		auto start_consume(void) -> std::expected<void, std::string>;
		auto stop_consume(void) -> std::expected<void, std::string>;
		auto disconnect(void) -> std::expected<void, std::string>;

		auto channel_open(int channel_id, const std::string& queue_name) -> std::expected<std::string, std::string>;
		auto channel_close(void) -> std::expected<void, std::string>;

		auto prepare_consume(void) -> std::expected<void, std::string>;
		auto register_consume(int target_channel_id,
							  const std::string& queue_name,
							  const std::function<std::expected<void, std::string>(const std::string&, const std::string&, const std::string&)>& callback)
			-> std::expected<void, std::string>;
		auto unregister_consume(int target_channel_id, const std::string& queue_name) -> std::expected<void, std::string>;

	protected:
		auto declare_queue(void) -> std::expected<std::string, std::string>;
		auto redeclare_channel(void) -> std::expected<void, std::string> override;

	private:
		std::optional<std::tuple<int, std::string>> declare_;
	};
}