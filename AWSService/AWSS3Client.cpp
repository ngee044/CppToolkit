#include "AWSS3Client.h"
#include "aws/s3/model/CreateBucketRequest.h"
#include "aws/s3/model/DeleteBucketRequest.h"
#include "aws/s3/model/DeleteObjectRequest.h"
#include "aws/s3/model/GetObjectRequest.h"
#include "aws/s3/model/HeadBucketRequest.h"
#include "aws/s3/model/HeadObjectRequest.h"
#include "aws/s3/model/ListObjectsRequest.h"
#include "aws/s3/model/PutObjectRequest.h"

#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/endpoint/EndpointParameter.h>
#include <aws/core/utils/memory/stl/AWSAllocator.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>

#include <format>
#include <fstream>

namespace AWSService
{
	AWSS3Client::AWSS3Client(const Aws::Client::ClientConfiguration& config) : s3_client_(config), client_config_(config) {}

	AWSS3Client::AWSS3Client(const Aws::Client::ClientConfiguration& config, bool use_virtual_addressing) : client_config_(config)
	{
		s3_client_ = Aws::S3::S3Client(client_config_, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, use_virtual_addressing);
	}

	AWSS3Client::AWSS3Client(const Aws::String& access_key_id, const Aws::String& secret_key, const Aws::String& target_region)
		: credentials_(access_key_id, secret_key), client_config_()
	{
		client_config_.region = target_region;
		s3_client_ = Aws::S3::S3Client(credentials_, nullptr, client_config_);
	}

	AWSS3Client::AWSS3Client(const Aws::String& access_key_id, const Aws::String& secret_key, const Aws::Client::ClientConfiguration& config)
		: credentials_(access_key_id, secret_key), client_config_(config)
	{
		// Use legacy constructor to support LocalStack with path-style addressing
		bool useVirtualAddressing = config.endpointOverride.empty(); // Use path-style for custom endpoints
		s3_client_ = Aws::S3::S3Client(credentials_, client_config_, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, useVirtualAddressing);
	}

	AWSS3Client::AWSS3Client(const Aws::String& access_key_id, const Aws::String& secret_key, const Aws::Client::ClientConfiguration& config, bool useVirtualAddressing)
		: credentials_(access_key_id, secret_key), client_config_(config)
	{
		// Use explicit virtual addressing setting
		s3_client_ = Aws::S3::S3Client(credentials_, client_config_, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, useVirtualAddressing);
	}

	AWSS3Client::~AWSS3Client() {}

	auto AWSS3Client::upload_file(const Aws::String& bucket_name, const Aws::String& object_name, const Aws::String& file_name) -> std::expected<void, std::string>
	{
		Aws::S3::Model::PutObjectRequest request;
		request.SetBucket(bucket_name);
		request.SetKey(object_name);

		auto input_data = Aws::MakeShared<Aws::FStream>("SampleAllocationTag", file_name.c_str(), std::ios_base::in | std::ios_base::binary);

		request.SetBody(input_data);

		Aws::S3::Model::PutObjectOutcome outcome;

		try
		{
			outcome = s3_client_.PutObject(request);
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to upload file: {}", e.what()));
		}

		if (!outcome.IsSuccess())
		{
			return std::unexpected(std::format("Failed to upload file: {}", outcome.GetError().GetMessage().c_str()));
		}

		return {};
	}

	auto AWSS3Client::download_file(const Aws::String& bucket_name, const Aws::String& object_name, const Aws::String& file_name) -> std::expected<void, std::string>
	{
		Aws::S3::Model::GetObjectRequest request;
		request.SetBucket(bucket_name);
		request.SetKey(object_name);

		Aws::S3::Model::GetObjectOutcome outcome;

		try
		{
			outcome = s3_client_.GetObject(request);
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to download file: {}", e.what()));
		}

		if (!outcome.IsSuccess())
		{
			return std::unexpected(std::format("Failed to download file: {}", outcome.GetError().GetMessage().c_str()));
		}

		Aws::OFStream download_file;
		download_file.open(file_name.c_str(), std::ios::out | std::ios::binary);
		download_file << outcome.GetResultWithOwnership().GetBody().rdbuf();

		return {};
	}

	auto AWSS3Client::delete_file(const Aws::String& bucket_name, const Aws::String& object_name) -> std::expected<void, std::string>
	{
		Aws::S3::Model::DeleteObjectRequest request;
		request.SetBucket(bucket_name);
		request.SetKey(object_name);

		Aws::S3::Model::DeleteObjectOutcome outcome;

		try
		{
			outcome = s3_client_.DeleteObject(request);
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to delete file: {}", e.what()));
		}

		if (!outcome.IsSuccess())
		{
			return std::unexpected(std::format("Failed to delete file: {}", outcome.GetError().GetMessage().c_str()));
		}

		return {};
	}

	auto AWSS3Client::file_exists(const Aws::String& bucket_name, const Aws::String& object_name) -> std::expected<bool, std::string>
	{
		Aws::S3::Model::HeadObjectRequest request;
		request.SetBucket(bucket_name);
		request.SetKey(object_name);

		Aws::S3::Model::HeadObjectOutcome outcome;

		try
		{
			outcome = s3_client_.HeadObject(request);
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to check if {} file exists: {}", object_name.c_str(), e.what()));
		}

		if (outcome.IsSuccess())
		{
			return true;
		}

		if (outcome.GetError().GetErrorType() == Aws::S3::S3Errors::NO_SUCH_KEY)
		{
			return false;
		}

		return std::unexpected(std::format("Failed to check if {} file exists: {}", object_name.c_str(), outcome.GetError().GetMessage().c_str()));
	}

	auto AWSS3Client::list_files(const Aws::String& bucket_name) -> std::expected<std::vector<std::string>, std::string>
	{
		Aws::S3::Model::ListObjectsRequest request;
		request.SetBucket(bucket_name);

		Aws::S3::Model::ListObjectsOutcome outcome;

		try
		{
			outcome = s3_client_.ListObjects(request);
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to list files: {}", e.what()));
		}

		if (!outcome.IsSuccess())
		{
			return std::unexpected(std::format("Failed to list files: {}", outcome.GetError().GetMessage().c_str()));
		}

		std::vector<std::string> file_names;
		for (const auto& object : outcome.GetResult().GetContents())
		{
			const auto& key = object.GetKey();
			file_names.emplace_back(key.data(), key.size());
		}

		return file_names;
	}

	auto AWSS3Client::list_buckets() -> std::expected<std::vector<std::string>, std::string>
	{
		Aws::S3::Model::ListBucketsOutcome outcome;

		try
		{
			outcome = s3_client_.ListBuckets();
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to list buckets: {}", e.what()));
		}

		if (!outcome.IsSuccess())
		{
			return std::unexpected(std::format("Failed to list buckets: {}", outcome.GetError().GetMessage().c_str()));
		}

		std::vector<std::string> bucket_names;
		for (const auto& bucket : outcome.GetResult().GetBuckets())
		{
			const auto& name = bucket.GetName();
			bucket_names.emplace_back(name.data(), name.size());
		}

		return bucket_names;
	}

	auto AWSS3Client::bucket_exists(const Aws::String& bucket_name) -> std::expected<bool, std::string>
	{
		Aws::S3::Model::HeadBucketRequest request;
		request.SetBucket(bucket_name);

		Aws::S3::Model::HeadBucketOutcome outcome;

		try
		{
			outcome = s3_client_.HeadBucket(request);
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to check if {} bucket exists: {}", bucket_name.c_str(), e.what()));
		}

		if (outcome.IsSuccess())
		{
			return true;
		}

		if (outcome.GetError().GetErrorType() == Aws::S3::S3Errors::NO_SUCH_BUCKET)
		{
			return false;
		}

		return std::unexpected(std::format("Failed to check if {} bucket exists: {}", bucket_name.c_str(), outcome.GetError().GetMessage().c_str()));
	}

	auto AWSS3Client::create_bucket(const Aws::String& bucket_name) -> std::expected<void, std::string>
	{
		Aws::S3::Model::CreateBucketRequest request;
		request.SetBucket(bucket_name);

		if (client_config_.region == Aws::Region::US_EAST_1)
		{
			Aws::S3::Model::CreateBucketConfiguration configuration;
			configuration.WithLocationConstraint(Aws::S3::Model::BucketLocationConstraintMapper::GetBucketLocationConstraintForName(client_config_.region));
			request.SetCreateBucketConfiguration(configuration);
		}

		Aws::S3::Model::CreateBucketOutcome outcome;

		try
		{
			outcome = s3_client_.CreateBucket(request);
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to create bucket: {}", e.what()));
		}

		if (!outcome.IsSuccess())
		{
			return std::unexpected(std::format("Failed to create bucket: {}", outcome.GetError().GetMessage().c_str()));
		}

		return {};
	}

	auto AWSS3Client::delete_bucket(const Aws::String& bucket_name) -> std::expected<void, std::string>
	{
		Aws::S3::Model::DeleteBucketRequest request;
		request.SetBucket(bucket_name);

		Aws::S3::Model::DeleteBucketOutcome outcome;

		try
		{
			outcome = s3_client_.DeleteBucket(request);
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to delete bucket: {}", e.what()));
		}

		if (!outcome.IsSuccess())
		{
			return std::unexpected(std::format("Failed to delete bucket {} : {}", bucket_name.c_str(), outcome.GetError().GetMessage().c_str()));
		}

		return {};
	}

	auto AWSS3Client::generate_presigned_url(const Aws::String& bucket_name,
											 const Aws::String& object_name,
											 const int& expiration_in_seconds,
											 const Aws::Http::HttpMethod& target_method) -> std::expected<std::string, std::string>
	{
		Aws::String url;

		try
		{
			url = s3_client_.GeneratePresignedUrl(bucket_name, object_name, target_method, expiration_in_seconds);
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to generate presigned URL: {}", e.what()));
		}

		if (url.empty())
		{
			return std::unexpected("Failed to generate presigned URL: empty URL returned");
		}

		return std::string(url.c_str(), url.size());
	}
} // namespace AWSService
