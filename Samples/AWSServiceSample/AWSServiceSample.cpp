#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <format>
#include <expected>

#include "Logger.h"
#include "LogTypes.h"
#include "ArgumentParser.h"

#include "AWSS3Client.h"
#include "AWSSQSPublisher.h"
#include "AWSSQSConsumer.h"

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/Scheme.h>

using namespace Utilities;
using namespace AWSService;

#ifdef _DEBUG
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Sequence;
#else
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Information;
#endif

std::string access_key_;
std::string secret_key_;
std::string region_ = "us-east-1";
std::string endpoint_;		// e.g. http://localhost:4566 (LocalStack); empty = real AWS
std::string mode_ = "both"; // s3 | sqs | both

std::string bucket_ = "cpptoolkit-sample-bucket";
std::string object_key_ = "sample.txt";

std::string queue_url_;
std::string message_ = "Hello from CppToolkit AWSService sample";
std::string message_group_id_; // required for FIFO (*.fifo) queues, leave empty for standard
std::string message_deduplication_id_;
int consume_seconds_ = 5;

std::string program_folder_;

auto parse_arguments(ArgumentParser& arguments) -> void;
auto print_help_message() -> void;
auto make_client_config() -> Aws::Client::ClientConfiguration;
auto run_s3_demo() -> void;
auto run_sqs_demo() -> void;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	if (arguments.to_string("--help") != std::nullopt)
	{
		print_help_message();
		return 0;
	}

	parse_arguments(arguments);
	program_folder_ = arguments.program_folder();

	if (mode_ != "s3" && mode_ != "sqs" && mode_ != "both")
	{
		std::cout << "Error: --mode must be one of: s3, sqs, both" << std::endl;
		print_help_message();
		return 0;
	}

	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(program_folder_);
	Logger::handle().start("AWSServiceSample");

	// The AWS SDK for C++ must be initialised once before any client is used and
	// shut down once after every client is destroyed. The AWSService library does
	// NOT do this for you, so the application owns the lifecycle.
	Aws::SDKOptions options;
	Aws::InitAPI(options);

	if (mode_ == "s3" || mode_ == "both")
	{
		run_s3_demo();
	}

	if (mode_ == "sqs" || mode_ == "both")
	{
		run_sqs_demo();
	}

	Aws::ShutdownAPI(options);

	Logger::handle().write(LogTypes::Information, "AWSServiceSample completed.");
	Logger::handle().stop();
	Logger::destroy();

	return 0;
}

auto make_client_config() -> Aws::Client::ClientConfiguration
{
	Aws::Client::ClientConfiguration config;
	config.region = region_.c_str();

	if (!endpoint_.empty())
	{
		// LocalStack / custom S3-compatible endpoints speak plain HTTP and use a
		// self-signed (or no) certificate, so disable TLS verification here.
		config.endpointOverride = endpoint_.c_str();
		config.scheme = Aws::Http::Scheme::HTTP;
		config.verifySSL = false;
	}

	return config;
}

auto run_s3_demo() -> void
{
	Logger::handle().write(LogTypes::Information, "=== S3 demo ===");

	auto config = make_client_config();

	// With explicit keys use the credential-based constructor; otherwise fall back
	// to the default credential provider chain (env vars, ~/.aws, IAM role, ...).
	// For a custom endpoint (LocalStack) path-style addressing is required.
	std::unique_ptr<AWSS3Client> s3;
	if (!access_key_.empty())
	{
		s3 = std::make_unique<AWSS3Client>(Aws::String(access_key_.c_str()), Aws::String(secret_key_.c_str()), config);
	}
	else
	{
		s3 = std::make_unique<AWSS3Client>(config, /*use_virtual_addressing=*/endpoint_.empty());
	}

	const Aws::String bucket(bucket_.c_str());
	const Aws::String key(object_key_.c_str());

	auto exists = s3->bucket_exists(bucket);
	if (!exists)
	{
		Logger::handle().write(LogTypes::Error, std::format("bucket_exists failed: {}", exists.error()));
		return;
	}
	if (!exists.value())
	{
		auto created = s3->create_bucket(bucket);
		if (!created)
		{
			Logger::handle().write(LogTypes::Error, std::format("create_bucket failed: {}", created.error()));
			return;
		}
		Logger::handle().write(LogTypes::Information, std::format("bucket created: {}", bucket_));
	}
	else
	{
		Logger::handle().write(LogTypes::Information, std::format("bucket already exists: {}", bucket_));
	}

	const std::string upload_path = std::format("{}/aws_sample_upload.txt", program_folder_);
	{
		std::ofstream out(upload_path, std::ios::out | std::ios::binary | std::ios::trunc);
		out << "CppToolkit AWSService S3 sample payload\n";
	}

	auto uploaded = s3->upload_file(bucket, key, Aws::String(upload_path.c_str()));
	if (!uploaded)
	{
		Logger::handle().write(LogTypes::Error, std::format("upload_file failed: {}", uploaded.error()));
		return;
	}
	Logger::handle().write(LogTypes::Information, std::format("uploaded: {} -> s3://{}/{}", upload_path, bucket_, object_key_));

	auto files = s3->list_files(bucket);
	if (!files)
	{
		Logger::handle().write(LogTypes::Error, std::format("list_files failed: {}", files.error()));
	}
	else
	{
		for (const auto& name : files.value())
		{
			Logger::handle().write(LogTypes::Information, std::format("object: {}", name));
		}
	}

	const std::string download_path = std::format("{}/aws_sample_download.txt", program_folder_);
	auto downloaded = s3->download_file(bucket, key, Aws::String(download_path.c_str()));
	if (!downloaded)
	{
		Logger::handle().write(LogTypes::Error, std::format("download_file failed: {}", downloaded.error()));
	}
	else
	{
		Logger::handle().write(LogTypes::Information, std::format("downloaded to: {}", download_path));
	}

	auto url = s3->generate_presigned_url(bucket, key, 3600);
	if (!url)
	{
		Logger::handle().write(LogTypes::Error, std::format("generate_presigned_url failed: {}", url.error()));
	}
	else
	{
		Logger::handle().write(LogTypes::Information, std::format("presigned GET url (1h): {}", url.value()));
	}

	auto deleted = s3->delete_file(bucket, key);
	if (!deleted)
	{
		Logger::handle().write(LogTypes::Error, std::format("delete_file failed: {}", deleted.error()));
	}
	else
	{
		Logger::handle().write(LogTypes::Information, std::format("deleted object: {}", object_key_));
	}
}

auto run_sqs_demo() -> void
{
	Logger::handle().write(LogTypes::Information, "=== SQS demo ===");

	if (queue_url_.empty())
	{
		Logger::handle().write(LogTypes::Error, "SQS demo skipped: --queue_url is required.");
		return;
	}

	auto config = make_client_config();

	// Publisher: send_message is synchronous and does not require start().
	std::unique_ptr<AWSSQSPublisher> publisher;
	if (!access_key_.empty())
	{
		publisher = std::make_unique<AWSSQSPublisher>(access_key_, secret_key_, config);
	}
	else
	{
		publisher = std::make_unique<AWSSQSPublisher>(config);
	}
	// Send to an explicitly named queue. (The sqs_url()-based send_message overload
	// is the alternative when the publisher targets a single preconfigured queue.)
	auto sent = publisher->send_message_to(Aws::String(queue_url_.c_str()), Aws::String(message_.c_str()), Aws::String(message_group_id_.c_str()),
										   Aws::String(message_deduplication_id_.c_str()));
	if (!sent)
	{
		Logger::handle().write(LogTypes::Error, std::format("send_message failed: {}", sent.error()));
	}
	else
	{
		Logger::handle().write(LogTypes::Information, std::format("message sent: {}", message_));
	}

	// Consumer: polls in a background ThreadPool job, invoking the handler per message.
	AWSSQSConsumerConfig consume_config;
	consume_config.wait_time_seconds = 2; // short long-poll so the run window stays responsive
	consume_config.visibility_timeout = 30;
	consume_config.max_number_of_messages = 5;

	std::unique_ptr<AWSSQSConsumer> consumer;
	if (!access_key_.empty())
	{
		consumer = std::make_unique<AWSSQSConsumer>(access_key_, secret_key_, config, consume_config);
	}
	else
	{
		consumer = std::make_unique<AWSSQSConsumer>(config, consume_config);
	}
	consumer->sqs_url(queue_url_);

	// Returning {} deletes the message; std::unexpected keeps it for redelivery.
	auto handler_registered = consumer->register_consume_handler(
		[](const std::string& body) -> std::expected<void, std::string>
		{
			Logger::handle().write(LogTypes::Information, std::format("[consumer] received: {}", body));
			return {};
		});
	if (!handler_registered)
	{
		Logger::handle().write(LogTypes::Error, std::format("register_consume_handler failed: {}", handler_registered.error()));
		return;
	}

	auto started = consumer->start();
	if (!started)
	{
		Logger::handle().write(LogTypes::Error, std::format("consumer start failed: {}", started.error()));
		return;
	}

	auto consuming = consumer->start_consume();
	if (!consuming)
	{
		Logger::handle().write(LogTypes::Error, std::format("start_consume failed: {}", consuming.error()));
		consumer->stop();
		return;
	}

	Logger::handle().write(LogTypes::Information, std::format("consuming for {} seconds...", consume_seconds_));
	std::this_thread::sleep_for(std::chrono::seconds(consume_seconds_));

	auto stopped = consumer->stop_consume();
	if (!stopped)
	{
		Logger::handle().write(LogTypes::Error, std::format("stop_consume failed: {}", stopped.error()));
	}
	consumer->stop();

	Logger::handle().write(LogTypes::Information, "SQS demo finished.");
}

auto parse_arguments(ArgumentParser& arguments) -> void
{
	auto int_target = arguments.to_int("--write_console_log");
	if (int_target != std::nullopt)
	{
		write_console_ = (LogTypes)int_target.value();
	}

	int_target = arguments.to_int("--write_file_log");
	if (int_target != std::nullopt)
	{
		write_file_ = (LogTypes)int_target.value();
	}

	int_target = arguments.to_int("--consume_seconds");
	if (int_target != std::nullopt)
	{
		consume_seconds_ = int_target.value();
	}

	auto string_target = arguments.to_string("--access_key");
	if (string_target != std::nullopt)
	{
		access_key_ = string_target.value();
	}

	string_target = arguments.to_string("--secret_key");
	if (string_target != std::nullopt)
	{
		secret_key_ = string_target.value();
	}

	string_target = arguments.to_string("--region");
	if (string_target != std::nullopt)
	{
		region_ = string_target.value();
	}

	string_target = arguments.to_string("--endpoint");
	if (string_target != std::nullopt)
	{
		endpoint_ = string_target.value();
	}

	string_target = arguments.to_string("--mode");
	if (string_target != std::nullopt)
	{
		mode_ = string_target.value();
	}

	string_target = arguments.to_string("--bucket");
	if (string_target != std::nullopt)
	{
		bucket_ = string_target.value();
	}

	string_target = arguments.to_string("--object_key");
	if (string_target != std::nullopt)
	{
		object_key_ = string_target.value();
	}

	string_target = arguments.to_string("--queue_url");
	if (string_target != std::nullopt)
	{
		queue_url_ = string_target.value();
	}

	string_target = arguments.to_string("--message");
	if (string_target != std::nullopt)
	{
		message_ = string_target.value();
	}

	string_target = arguments.to_string("--message_group_id");
	if (string_target != std::nullopt)
	{
		message_group_id_ = string_target.value();
	}

	string_target = arguments.to_string("--message_deduplication_id");
	if (string_target != std::nullopt)
	{
		message_deduplication_id_ = string_target.value();
	}
}

auto print_help_message() -> void
{
	std::cout << "Usage: AWSServiceSample [OPTIONS]" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  --help                        Show this help message and exit" << std::endl;
	std::cout << "  --mode <s3|sqs|both>          Which demo to run (default: both)" << std::endl;
	std::cout << "  --access_key <key>            AWS access key id (omit to use the default credential chain)" << std::endl;
	std::cout << "  --secret_key <key>            AWS secret access key" << std::endl;
	std::cout << "  --region <region>             AWS region (default: us-east-1)" << std::endl;
	std::cout << "  --endpoint <url>              Custom endpoint, e.g. http://localhost:4566 for LocalStack" << std::endl;
	std::cout << "  --bucket <name>               S3 bucket name (default: cpptoolkit-sample-bucket)" << std::endl;
	std::cout << "  --object_key <key>            S3 object key (default: sample.txt)" << std::endl;
	std::cout << "  --queue_url <url>             SQS queue URL (required for sqs mode)" << std::endl;
	std::cout << "  --message <text>              SQS message body" << std::endl;
	std::cout << "  --message_group_id <id>       SQS message group id (required for FIFO queues)" << std::endl;
	std::cout << "  --message_deduplication_id <id> SQS dedup id (FIFO without content-based dedup)" << std::endl;
	std::cout << "  --consume_seconds <n>         Seconds to consume before stopping (default: 5)" << std::endl;
	std::cout << "  --write_console_log <level>   Console log level (default: 6 Debug/Sequence, 4 Information)" << std::endl;
	std::cout << "  --write_file_log <level>      File log level (default: 0 None)" << std::endl;
	std::cout << std::endl;
	std::cout << "Examples:" << std::endl;
	std::cout << "  # LocalStack (S3 + SQS)" << std::endl;
	std::cout << "  AWSServiceSample --endpoint http://localhost:4566 --access_key test --secret_key test \\" << std::endl;
	std::cout << "                   --bucket demo --queue_url http://localhost:4566/000000000000/demo-queue" << std::endl;
	std::cout << std::endl;
	std::cout << "  # Real AWS, S3 only, default credential chain" << std::endl;
	std::cout << "  AWSServiceSample --mode s3 --region ap-northeast-2 --bucket my-bucket" << std::endl;
}
