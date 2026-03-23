#pragma once

#include "Job.h"
#include "DataModes.h"

#include <expected>
#include <functional>

namespace Network
{
	class ReceivingJob : public Thread::Job
	{
	public:
		ReceivingJob(const std::vector<uint8_t>& data,
					 const std::function<std::expected<void, std::string>(
						 DataModes, const std::vector<uint8_t>&)>& callback);
		virtual ~ReceivingJob(void);

	private:
		auto working(void) -> std::expected<void, std::string> override;

	private:
		std::function<std::expected<void, std::string>(DataModes, const std::vector<uint8_t>&)>
			receiving_callback_;
	};
}