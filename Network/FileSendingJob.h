#pragma once

#include "Job.h"
#include "DataModes.h"

#include <functional>

namespace Network
{
	class FileSendingJob : public Thread::Job
	{
	public:
		FileSendingJob(const std::vector<uint8_t>& file_information,
					   const std::function<std::tuple<bool, std::optional<std::string>>(
						   const DataModes&, const std::vector<uint8_t>&)>& callback);
		virtual ~FileSendingJob(void);

	private:
		auto working(void) -> std::tuple<bool, std::optional<std::string>> override;

	private:
		std::function<std::tuple<bool, std::optional<std::string>>(const DataModes&, const std::vector<uint8_t>&)>
			sending_callback_;
	};
}