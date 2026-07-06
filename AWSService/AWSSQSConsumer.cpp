#include "AWSSQSConsumer.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <random>
#include <thread>

using namespace Thread;

namespace AWSService
{
	AWSSQSConsumer::AWSSQSConsumer(const Aws::Client::ClientConfiguration& config, const AWSSQSConsumerConfig& consume_config)
		: AWSSQSBase(config), consume_config_(consume_config), is_running_(false)
	{
	}

	AWSSQSConsumer::AWSSQSConsumer(const std::string& access_key_id,
								   const std::string& secret_key,
								   const std::string& target_region,
								   const AWSSQSConsumerConfig& consume_config)
		: AWSSQSBase(access_key_id, secret_key, target_region), consume_config_(consume_config), is_running_(false)
	{
	}

	AWSSQSConsumer::AWSSQSConsumer(const std::string& access_key_id,
								   const std::string& secret_key,
								   const Aws::Client::ClientConfiguration& config,
								   const AWSSQSConsumerConfig& consume_config)
		: AWSSQSBase(access_key_id, secret_key, config), consume_config_(consume_config), is_running_(false)
	{
	}

	AWSSQSConsumer::~AWSSQSConsumer() {}

	auto AWSSQSConsumer::register_consume_handler(std::function<std::expected<void, std::string>(const std::string&)> handler) -> std::expected<void, std::string>
	{
		handler_ = handler;

		return {};
	}

	auto AWSSQSConsumer::start_consume() -> std::expected<void, std::string>
	{
		if (thread_pool_ == nullptr)
		{
			return std::unexpected("thread pool is nullptr");
		}

		if (handler_ == nullptr)
		{
			return std::unexpected("handler is not initialized");
		}

		bool expected = false;
		if (!is_running_.compare_exchange_strong(expected, true))
		{
			return std::unexpected("consumer already running");
		}

		int max_msgs = consume_config_.max_number_of_messages;
		if (max_msgs <= 0)
		{
			max_msgs = 1;
		}
		max_msgs = std::clamp(max_msgs, 1, 10);

		auto polling_job = std::make_shared<Job>(
			JobPriorities::LongTerm,
			[this, max_msgs]() -> std::expected<void, std::string>
			{
				// Simple backoff helpers
				int empty_count = 0;
				int error_count = 0;
				std::mt19937 rng{ std::random_device{}() };

				auto jitter = [&](int ms) -> int
				{
					int amp = static_cast<int>(ms * 0.10);
					if (amp <= 0)
					{
						return std::max(ms, 0);
					}
					std::uniform_int_distribution<int> dist(-amp, amp);
					int with_jitter = ms + dist(rng);
					return std::max(0, with_jitter);
				};

				auto on_success = [&]() -> int
				{
					empty_count = 0;
					error_count = 0;
					return 0;
				};

				auto on_empty = [&]() -> int
				{
					empty_count = std::min(empty_count + 1, 1000);
					int delay = std::min(500 + empty_count * 500, 5000);
					return jitter(delay);
				};

				auto on_error = [&]() -> int
				{
					error_count = std::min(error_count + 1, 10);
					long long factor = 1LL << error_count;
					long long exp_ms = factor * 500LL;
					int delay = static_cast<int>(std::min(exp_ms, 30000LL));
					return jitter(delay);
				};

				while (is_running_.load())
				{
					auto result = receive_process_once(Aws::String(sqs_url().c_str()), consume_config_.wait_time_seconds, consume_config_.visibility_timeout, max_msgs);
					if (!result)
					{
						int d = on_error();
						std::this_thread::sleep_for(std::chrono::milliseconds(d));
						continue;
					}
					if (result.value())
					{
						int d = on_empty();
						std::this_thread::sleep_for(std::chrono::milliseconds(d));
						continue;
					}

					on_success();
				}

				return {};
			},
			"SQS_Receive_Loop");

		auto queued = thread_pool_->push(polling_job);
		if (!queued)
		{
			is_running_.store(false);
			return std::unexpected(queued.error());
		}

		return {};
	}

	auto AWSSQSConsumer::stop_consume() -> std::expected<void, std::string>
	{
		is_running_.store(false);
		if (thread_pool_ == nullptr)
		{
			return {};
		}

		return basic_stop_message();
	}

	auto AWSSQSConsumer::is_running() const -> bool { return is_running_.load(); }

} // namespace AWSService
