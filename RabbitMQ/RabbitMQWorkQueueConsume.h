#pragma once

#include "RabbitMQBase.h"

#include <functional>
#include <string>

namespace RabbitMQ
{
	class RabbitMQWorkQueueConsume : public RabbitMQBase
	{
	public:
		RabbitMQWorkQueueConsume(const std::string& host, int port, const std::string& user_name, const std::string& password, const SSLOptions& ssl_options = SSLOptions());

		// Queue policies (optional)
		void set_queue_policies(const std::optional<std::string>& dlx_exchange,
							  const std::optional<std::string>& dlx_routing_key,
							  const std::optional<uint32_t>& message_ttl_ms);

		auto connect(const int& heartbeat) -> std::tuple<bool, std::optional<std::string>>;
		auto start_consume(void) -> std::tuple<bool, std::optional<std::string>>;
		auto stop_consume(void) -> std::tuple<bool, std::optional<std::string>>;
		auto disconnect(void) -> std::tuple<bool, std::optional<std::string>>;

		auto channel_open(const int& channel_id, const std::string& queue_name) -> std::tuple<std::optional<std::string>, std::optional<std::string>>;
		auto channel_close(void) -> std::tuple<bool, std::optional<std::string>>;

		auto prepare_consume(void) -> std::tuple<bool, std::optional<std::string>>;
		auto register_consume(const int& target_channel_id,
							  const std::string& queue_name,
							  const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)>& callback)
			-> std::tuple<bool, std::optional<std::string>>;
		auto unregister_consume(const int& target_channel_id, const std::string& queue_name) -> std::tuple<bool, std::optional<std::string>>;

	protected:
		auto declare_queue(void) -> std::tuple<std::optional<std::string>, std::optional<std::string>>;
		auto redeclare_channel(void) -> std::tuple<bool, std::optional<std::string>> override;

	private:
		std::optional<std::tuple<int, std::string>> declare_;

		// Optional queue argument settings
		std::optional<std::string> dlx_exchange_;
		std::optional<std::string> dlx_routing_key_;
		std::optional<uint32_t> message_ttl_ms_;
	};
}
