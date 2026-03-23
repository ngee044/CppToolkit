#pragma once

#include "Job.h"
#include "DataModes.h"

#include <expected>
#include <functional>

namespace Network
{
	class FileSendingJob : public Thread::Job
	{
	public:
		FileSendingJob(const std::vector<uint8_t>& file_information,
					   const std::function<std::expected<void, std::string>(
						   DataModes, const std::vector<uint8_t>&)>& callback);
		virtual ~FileSendingJob(void);

	private:
		auto working(void) -> std::expected<void, std::string> override;

	private:
		std::function<std::expected<void, std::string>(DataModes, const std::vector<uint8_t>&)>
			sending_callback_;
	};
}