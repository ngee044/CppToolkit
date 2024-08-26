#pragma once

#include "Job.h"
#include "DataModes.h"

#include <functional>

namespace Network
{
	class ReceivingJob : public Thread::Job
	{
	public:
		ReceivingJob(const std::vector<uint8_t>& data,
					 const std::function<std::tuple<bool, std::optional<std::string>>(
						 const DataModes&, const std::vector<uint8_t>&)>& callback);
		virtual ~ReceivingJob(void);

	private:
		auto working(void) -> std::tuple<bool, std::optional<std::string>> override;

	private:
		std::function<std::tuple<bool, std::optional<std::string>>(const DataModes&, const std::vector<uint8_t>&)>
			receiving_callback_;
	};
}