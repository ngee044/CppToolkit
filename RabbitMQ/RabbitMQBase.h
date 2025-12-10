#pragma once

#include "ThreadPool.h"
#include "SSLOptions.h"
#include "ConsumeInformationContainer.h"

#if __has_include(<rabbitmq-c/amqp.h>)
	#include <rabbitmq-c/amqp.h>
#else
	#include <amqp.h>
#endif

#if __has_include(<rabbitmq-c/ssl_socket.h>)
	#include <rabbitmq-c/ssl_socket.h>
#elif __has_include(<rabbitmq-c/amqp_ssl_socket.h>)
	#include <rabbitmq-c/amqp_ssl_socket.h>
#elif __has_include(<amqp_ssl_socket.h>)
	#include <amqp_ssl_socket.h>
#else
	#error "Missing rabbitmq-c SSL socket header"
#endif

#if __has_include(<rabbitmq-c/tcp_socket.h>)
	#include <rabbitmq-c/tcp_socket.h>
#elif __has_include(<rabbitmq-c/amqp_tcp_socket.h>)
	#include <rabbitmq-c/amqp_tcp_socket.h>
#elif __has_include(<amqp_tcp_socket.h>)
	#include <amqp_tcp_socket.h>
#else
	#error "Missing rabbitmq-c TCP socket header"
#endif

#include <map>
#include <mutex>
#include <tuple>
#include <future>
#include <memory>
#include <string>
#include <optional>
#include <functional>

using namespace Thread;

namespace RabbitMQ
{
	enum class DeliveryMode : uint8_t
	{
		NonPersistent = 1,
		Persistent = 2
	};

	class RabbitMQBase
	{
	public:
		RabbitMQBase(const std::string& host, int port, const std::string& user_name, const std::string& password, const SSLOptions& ssl_options = SSLOptions());
		virtual ~RabbitMQBase();

		auto start() -> std::tuple<bool, std::optional<std::string>>;
		auto wait_stop() -> std::tuple<bool, std::optional<std::string>>;
		auto stop() -> void;

	protected:
		auto basic_publish(int target_channel_id,
			const std::string& exchange,
			const std::string& routing_key,
			const std::string& message,
			const std::string& exchange_mode,
			const std::string& content_type = "text/plain",
			const DeliveryMode& delivery_mode = DeliveryMode::Persistent,
			const bool& use_confirm_select = true,
			const std::optional<uint32_t>& expiration_millisecond = std::nullopt) -> std::tuple<bool, std::optional<std::string>>;
		auto basic_register_consume(int target_channel_id,
			const std::string& target_queue_name,
			const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)>& callback) -> std::tuple<bool, std::optional<std::string>>;
		auto basic_unregister_consume(const int& target_channel_id, const std::string& target_queue) -> std::tuple<bool, std::optional<std::string>>;
		auto basic_connect(int heartbeat = 60) -> std::tuple<bool, std::optional<std::string>>;
		auto basic_disconnect() -> std::tuple<bool, std::optional<std::string>>;
		auto basic_start_consume() -> std::tuple<bool, std::optional<std::string>>;
		auto basic_stop_consume() -> std::tuple<bool, std::optional<std::string>>;
		auto basic_ssl_setup(amqp_socket_t* socket) -> std::tuple<bool, std::optional<std::string>>;
		auto basic_login(amqp_socket_t* socket, const std::string& socket_type, const int& heartbeat = 60) -> std::tuple<bool, std::optional<std::string>>;
		auto basic_start(amqp_socket_t* socket, const std::string& socket_type) -> std::tuple<bool, std::optional<std::string>>;
		auto basic_channel_open(int channel_id) -> std::tuple<bool, std::optional<std::string>>;
		auto basic_channel_close(int channel_id) -> std::tuple<bool, std::optional<std::string>>;
		auto basic_declare_queue(amqp_connection_state_t conn,
								int target_channel_id,
								const std::string& queue_name,
								bool passive = false,
								bool durable = false,
								bool exclusive = false,
								bool auto_delete = false) -> std::tuple<std::optional<std::string>, std::optional<std::string>>;
		auto basic_delete_queue(amqp_connection_state_t conn, int target_channel_id, const std::string& queue_name, bool if_unused = false, bool if_empty = false) -> std::tuple<bool, std::optional<std::string>>;
		auto basic_bind_queue(amqp_connection_state_t conn, int target_channel_id, const std::string& queue_name, const std::string& exchange, const std::string& routing_key) -> std::tuple<bool, std::optional<std::string>>;

		auto create_thread_pool() -> std::tuple<bool, std::optional<std::string>>;
		auto destroy_thread_pool() -> std::tuple<bool, std::optional<std::string>>;
		
		auto status_message(const amqp_status_enum_& status) -> std::string;
		auto reply_message(const amqp_rpc_reply_t& reply) -> std::string;

		virtual auto redeclare_channel(void) -> std::tuple<bool, std::optional<std::string>> { return { false, "Not implemented yet" }; }
	
	private:
		auto register_consumer(const int& target_channel_id, const std::string& target_queue) -> std::tuple<bool, std::optional<std::string>>;
		auto unregister_consumer(const int& target_channel_id) -> std::tuple<bool, std::optional<std::string>>;
		auto reconnect(void) -> std::tuple<bool, std::optional<std::string>>;

	protected:
		int port_;

		std::mutex mutex_;

		std::string host_;
		std::string user_name_;
		std::string password_;
		SSLOptions ssl_options_;

		amqp_connection_state_t conn_;

		std::promise<void> stop_promise_;
		std::optional<std::future<void>> stop_future_;
		std::atomic<bool> continue_receiving_{ false };

		std::shared_ptr<ThreadPool> thread_pool_;
		std::unique_ptr<ConsumeInformationContainer> consume_information_container_;
	};
}
