#include "SendingJob.h"

#include "Logger.h"
#include "NetworkConstexpr.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include <algorithm>

using namespace Thread;
using namespace Utilities;

namespace Network
{
	SendingJob::SendingJob(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
						   const std::vector<uint8_t>& start_code,
						   const std::vector<uint8_t>& data,
						   const std::vector<uint8_t>& end_code,
						   const size_t& buffer_size)
		: Job(JobPriorities::Top, data, "SendingJob")
		, socket_(socket)
		, start_code_(start_code)
		, end_code_(end_code)
		, buffer_size_(buffer_size)
	{
	}

	SendingJob::~SendingJob(void) {}

	auto SendingJob::working(void) -> std::tuple<bool, std::optional<std::string>>
	{
		std::vector<uint8_t> data = get_data();
		if (data.empty())
		{
			return { false, "cannot send to empty data" };
		}

		if (socket_ == nullptr)
		{
			return { false, "cannot send on null socket" };
		}

		try
		{
			auto [start_result, start_error] = send_start();
			if (!start_result)
			{
				return { start_result, start_error };
			}

			auto [length_result, length_error] = send_length(data.size());
			if (!length_result)
			{
				return { length_result, length_error };
			}

			auto [data_result, data_error] = send_data(data);
			if (!data_result)
			{
				return { data_result, data_error };
			}

			auto [end_result, end_error] = send_end();
			if (!end_result)
			{
				return { end_result, end_error };
			}

			return { true, std::nullopt };
		}
		catch (const std::overflow_error& error)
		{
			return { false, error.what() };
		}
		catch (const std::runtime_error& error)
		{
			return { false, error.what() };
		}
		catch (const std::exception& error)
		{
			return { false, error.what() };
		}
		catch (...)
		{
			return { false, "unknown error" };
		}
	}

	auto SendingJob::send_start(void) -> std::tuple<bool, std::optional<std::string>>
	{
		size_t sent_size = socket_->send(boost::asio::buffer(start_code_.data(), start_code_.size()));
		if (sent_size != start_code_.size())
		{
			return { false, fmt::format("cannot send start code : {} bytes", start_code_.size()) };
		}

		return { true, std::nullopt };
	}

	auto SendingJob::send_length(const uint64_t& length) -> std::tuple<bool, std::optional<std::string>>
	{
		size_t sent_size = socket_->send(boost::asio::buffer(&length, LENGTH_SIZE));
		if (sent_size != sizeof(uint64_t))
		{
			return { false, fmt::format("cannot send length code : {} bytes", LENGTH_SIZE) };
		}

		return { true, std::nullopt };
	}

	auto SendingJob::send_data(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
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
				return { false, fmt::format("cannot send data: sent [{}] / total [{}] bytes", temp, count) };
			}

			index += temp;
		}

		return { true, std::nullopt };
	}

	auto SendingJob::send_end(void) -> std::tuple<bool, std::optional<std::string>>
	{
		size_t sent_size = socket_->send(boost::asio::buffer(end_code_.data(), end_code_.size()));
		if (sent_size != end_code_.size())
		{
			return { false, fmt::format("cannot send end code : {} bytes", end_code_.size()) };
		}

		return { true, std::nullopt };
	}
}