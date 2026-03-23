#include "SendingJob.h"

#include "Logger.h"
#include "NetworkConstexpr.h"

#include <format>

#include <algorithm>

using namespace Thread;
using namespace Utilities;

namespace Network
{
	SendingJob::SendingJob(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
						   const std::vector<uint8_t>& start_code,
						   const std::vector<uint8_t>& data,
						   const std::vector<uint8_t>& end_code,
						   size_t buffer_size)
		: Job(JobPriorities::Top, data, "SendingJob")
		, socket_(socket)
		, start_code_(start_code)
		, end_code_(end_code)
		, buffer_size_(buffer_size)
	{
	}

	SendingJob::~SendingJob(void) {}

	auto SendingJob::working(void) -> std::expected<void, std::string>
	{
		std::vector<uint8_t> data = get_data();
		if (data.empty())
		{
			return std::unexpected("cannot send to empty data");
		}

		if (socket_ == nullptr)
		{
			return std::unexpected("cannot send on null socket");
		}

		try
		{
			auto start_result = send_start();
			if (!start_result)
			{
				return start_result;
			}

			auto length_result = send_length(data.size());
			if (!length_result)
			{
				return length_result;
			}

			auto data_result = send_data(data);
			if (!data_result)
			{
				return data_result;
			}

			auto end_result = send_end();
			if (!end_result)
			{
				return end_result;
			}

			return {};
		}
		catch (const std::overflow_error& error)
		{
			return std::unexpected(std::string(error.what()));
		}
		catch (const std::runtime_error& error)
		{
			return std::unexpected(std::string(error.what()));
		}
		catch (const std::exception& error)
		{
			return std::unexpected(std::string(error.what()));
		}
		catch (...)
		{
			return std::unexpected(std::string("unknown error"));
		}
	}

	auto SendingJob::send_start(void) -> std::expected<void, std::string>
	{
		size_t sent_size = socket_->send(boost::asio::buffer(start_code_.data(), start_code_.size()));
		if (sent_size != start_code_.size())
		{
			return std::unexpected(std::format("cannot send start code : {} bytes", start_code_.size()));
		}

		return {};
	}

	auto SendingJob::send_length(const uint64_t& length) -> std::expected<void, std::string>
	{
		size_t sent_size = socket_->send(boost::asio::buffer(&length, LENGTH_SIZE));
		if (sent_size != sizeof(uint64_t))
		{
			return std::unexpected(std::format("cannot send length code : {} bytes", LENGTH_SIZE));
		}

		return {};
	}

	auto SendingJob::send_data(const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		size_t temp = 0;
		size_t count = data.size();
		for (size_t index = 0; index < count;)
		{
			temp = std::min(buffer_size_, count - index);

			std::vector<uint8_t> temp_buffer(data.begin() + index, data.begin() + index + temp);
			temp = socket_->send(boost::asio::buffer(temp_buffer.data(), temp));
			if (temp == 0)
			{
				return std::unexpected(std::format("cannot send data: sent [{}] / total [{}] bytes", temp, count));
			}

			index += temp;
		}

		return {};
	}

	auto SendingJob::send_end(void) -> std::expected<void, std::string>
	{
		size_t sent_size = socket_->send(boost::asio::buffer(end_code_.data(), end_code_.size()));
		if (sent_size != end_code_.size())
		{
			return std::unexpected(std::format("cannot send end code : {} bytes", end_code_.size()));
		}

		return {};
	}
}