#pragma once

#include "Job.h"

#include "boost/asio.hpp"

#include <memory>

namespace Network
{
	class SendingJob : public Thread::Job
	{
	public:
		SendingJob(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
				   const std::vector<uint8_t>& start_code,
				   const std::vector<uint8_t>& data,
				   const std::vector<uint8_t>& end_code,
				   const size_t& buffer_size);
		virtual ~SendingJob(void);

	private:
		auto working(void) -> std::tuple<bool, std::optional<std::string>> override;

		auto send_start(void) -> std::tuple<bool, std::optional<std::string>>;
		auto send_length(const uint64_t& length) -> std::tuple<bool, std::optional<std::string>>;
		auto send_data(const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>;
		auto send_end(void) -> std::tuple<bool, std::optional<std::string>>;

	private:
		std::vector<uint8_t> start_code_;
		std::vector<uint8_t> end_code_;
		size_t buffer_size_;
		std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
	};
}