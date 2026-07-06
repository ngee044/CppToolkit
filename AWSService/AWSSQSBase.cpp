#include "AWSSQSBase.h"
#include "ThreadWorker.h"

#include <format>

using namespace Thread;

namespace AWSService
{

	AWSSQSBase::AWSSQSBase(const Aws::Client::ClientConfiguration& config)
		: sqs_client_(config), client_config_(config), thread_pool_(nullptr), stop_future_(std::nullopt), handler_(nullptr)
	{
	}

	AWSSQSBase::AWSSQSBase(const std::string& access_key_id, const std::string& secret_key, const std::string& target_region)
		: credentials_(access_key_id, secret_key), client_config_(), thread_pool_(nullptr), stop_future_(std::nullopt), handler_(nullptr)
	{
		client_config_.region = target_region;
		sqs_client_ = Aws::SQS::SQSClient(credentials_, nullptr, client_config_);
	}

	AWSSQSBase::AWSSQSBase(const std::string& access_key_id, const std::string& secret_key, const Aws::Client::ClientConfiguration& config)
		: credentials_(access_key_id, secret_key), client_config_(config), thread_pool_(nullptr), stop_future_(std::nullopt), handler_(nullptr)
	{
		sqs_client_ = Aws::SQS::SQSClient(credentials_, nullptr, client_config_);
	}

	AWSSQSBase::~AWSSQSBase() {}

	auto AWSSQSBase::sqs_url() -> std::string { return sqs_url_; }

	auto AWSSQSBase::sqs_url(const std::string& sqs_url) -> void { sqs_url_ = sqs_url; }

	auto AWSSQSBase::start() -> std::expected<void, std::string>
	{
		stop();

		auto created = create_thread_pool();
		if (!created)
		{
			return std::unexpected(created.error());
		}

		return thread_pool_->start();
	}

	auto AWSSQSBase::stop() -> void
	{
		destroy_thread_pool();

		if (stop_future_ != std::nullopt)
		{
			stop_promise_.set_value();
		}
	}

	auto AWSSQSBase::wait_stop() -> std::expected<void, std::string>
	{
		// TODO
		// not used? then remove this function

		if (stop_future_ != std::nullopt)
		{
			return std::unexpected("already created future for wait stop");
		}

		stop_future_ = stop_promise_.get_future();
		stop_future_.value().wait();
		stop_future_.reset();

		return {};
	}

	auto AWSSQSBase::basic_send_message(const Aws::String& queue_url,
										const Aws::String& message_body,
										Aws::String message_group_id,
										Aws::String message_deduplication_id,
										int delay_seconds) -> std::expected<void, std::string>
	{
		Aws::SQS::Model::SendMessageRequest request;
		request.SetQueueUrl(queue_url);
		request.SetMessageBody(message_body);

		if (delay_seconds > 0)
		{
			request.SetDelaySeconds(delay_seconds);
		}

		if (!message_group_id.empty())
		{
			request.SetMessageGroupId(message_group_id);
		}

		if (!message_deduplication_id.empty())
		{
			request.SetMessageDeduplicationId(message_deduplication_id);
		}

		auto outcome = sqs_client_.SendMessage(request);
		if (!outcome.IsSuccess())
		{
			return std::unexpected(std::format("Failed to send message: {}", outcome.GetError().GetMessage().c_str()));
		}

		return {};
	}

	auto AWSSQSBase::receive_process_once(const Aws::String& queue_url, int wait_time_seconds, int visibility_timeout, int max_number_of_messages)
		-> std::expected<bool, std::string>
	{
		if (handler_ == nullptr)
		{
			return std::unexpected("handler is not registered");
		}

		Aws::SQS::Model::ReceiveMessageRequest request;
		request.SetQueueUrl(queue_url);
		request.SetWaitTimeSeconds(wait_time_seconds);
		request.SetVisibilityTimeout(visibility_timeout);
		request.SetMaxNumberOfMessages(max_number_of_messages);

		auto outcome = sqs_client_.ReceiveMessage(request);
		if (!outcome.IsSuccess())
		{
			return std::unexpected(std::format("Failed to receive message: {}", outcome.GetError().GetMessage().c_str()));
		}

		const auto& messages = outcome.GetResult().GetMessages();
		if (messages.empty())
		{
			return true;
		}

		for (const auto& message : messages)
		{
			const auto& body = message.GetBody();
			std::string message_body(body.data(), body.size());

			std::expected<void, std::string> handled;
			try
			{
				handled = handler_(message_body);
			}
			catch (const std::exception& e)
			{
				return std::unexpected(std::format("handler exception: {}", e.what()));
			}
			catch (...)
			{
				return std::unexpected("unknown handler exception");
			}

			if (!handled)
			{
				return std::unexpected(handled.error());
			}

			Aws::SQS::Model::DeleteMessageRequest delete_request;
			delete_request.SetQueueUrl(queue_url);
			delete_request.SetReceiptHandle(message.GetReceiptHandle());

			auto delete_response = sqs_client_.DeleteMessage(delete_request);
			if (!delete_response.IsSuccess())
			{
				return std::unexpected(std::string(delete_response.GetError().GetMessage().c_str()));
			}
		}

		return false;
	}

	auto AWSSQSBase::basic_receive_message(const Aws::String& queue_url, int wait_time_seconds, int visibility_timeout, int max_number_of_messages)
		-> std::expected<void, std::string>
	{
		if (handler_ == nullptr)
		{
			return std::unexpected("handler is not registered");
		}

		return thread_pool_->push(std::make_shared<Job>(
			JobPriorities::LongTerm,
			[this, queue_url, wait_time_seconds, visibility_timeout, max_number_of_messages]() -> std::expected<void, std::string>
			{
				auto result = receive_process_once(queue_url, wait_time_seconds, visibility_timeout, max_number_of_messages);
				if (!result)
				{
					return std::unexpected(result.error());
				}
				return {};
			},
			"ReceiveMessageJob"));
	}

	auto AWSSQSBase::basic_stop_message() -> std::expected<void, std::string>
	{
		if (thread_pool_ == nullptr)
		{
			return std::unexpected("thread pool is nullptr");
		}

		return thread_pool_->stop();
	}

	auto AWSSQSBase::create_thread_pool() -> std::expected<void, std::string>
	{
		auto destroyed = destroy_thread_pool();
		if (!destroyed)
		{
			return std::unexpected(destroyed.error());
		}

		try
		{
			thread_pool_ = std::make_unique<ThreadPool>();
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("[SQSConsumer]Failed to create thread pool: {}", e.what()));
		}

		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::LongTerm }));

		return {};
	}

	auto AWSSQSBase::destroy_thread_pool() -> std::expected<void, std::string>
	{
		if (thread_pool_ == nullptr)
		{
			return {};
		}

		auto stopped = thread_pool_->stop();
		if (!stopped)
		{
			return std::unexpected(stopped.error());
		}

		thread_pool_.reset();

		return {};
	}
} // namespace AWSService
