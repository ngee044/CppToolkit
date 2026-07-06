#pragma once

#include <aws/core/Region.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpTypes.h>
#include <aws/s3/S3Client.h>

#include <expected>
#include <string>
#include <vector>

namespace AWSService
{
	class AWSS3Client
	{
	public:
		AWSS3Client(const Aws::Client::ClientConfiguration& config = {});
		AWSS3Client(const Aws::Client::ClientConfiguration& config, bool use_virtual_addressing);
		AWSS3Client(const Aws::String& access_key_id, const Aws::String& secret_key, const Aws::String& target_region = Aws::Region::US_EAST_1);
		AWSS3Client(const Aws::String& access_key_id, const Aws::String& secret_key, const Aws::Client::ClientConfiguration& config);
		AWSS3Client(const Aws::String& access_key_id, const Aws::String& secret_key, const Aws::Client::ClientConfiguration& config, bool useVirtualAddressing);
		~AWSS3Client();

		auto upload_file(const Aws::String& bucket_name, const Aws::String& object_name, const Aws::String& file_name) -> std::expected<void, std::string>;
		auto download_file(const Aws::String& bucket_name, const Aws::String& object_name, const Aws::String& file_name) -> std::expected<void, std::string>;
		auto delete_file(const Aws::String& bucket_name, const Aws::String& object_name) -> std::expected<void, std::string>;
		auto file_exists(const Aws::String& bucket_name, const Aws::String& object_name) -> std::expected<bool, std::string>;

		auto list_files(const Aws::String& bucket_name) -> std::expected<std::vector<std::string>, std::string>;
		auto list_buckets() -> std::expected<std::vector<std::string>, std::string>;

		auto bucket_exists(const Aws::String& bucket_name) -> std::expected<bool, std::string>;
		auto create_bucket(const Aws::String& bucket_name) -> std::expected<void, std::string>;
		auto delete_bucket(const Aws::String& bucket_name) -> std::expected<void, std::string>;

		auto generate_presigned_url(const Aws::String& bucket_name,
									const Aws::String& object_name,
									const int& expiration_in_seconds,
									const Aws::Http::HttpMethod& target_method = Aws::Http::HttpMethod::HTTP_GET) -> std::expected<std::string, std::string>;

	private:
		Aws::S3::S3Client s3_client_;
		Aws::Auth::AWSCredentials credentials_;
		Aws::Client::ClientConfiguration client_config_;
	};
} // namespace AWSService
