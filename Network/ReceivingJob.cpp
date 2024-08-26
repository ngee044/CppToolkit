#include "ReceivingJob.h"

#include "Logger.h"
#include "Combiner.h"

#include "fmt/xchar.h"
#include "fmt/format.h"

using namespace Thread;
using namespace Utilities;

namespace Network
{
	ReceivingJob::ReceivingJob(const std::vector<uint8_t>& data,
							   const std::function<std::tuple<bool, std::optional<std::string>>(
								   const DataModes&, const std::vector<uint8_t>&)>& callback)
		: Job(JobPriorities::High, data, "ReceivingJob"), receiving_callback_(callback)
	{
	}

	ReceivingJob::~ReceivingJob(void) {}

	auto ReceivingJob::working(void) -> std::tuple<bool, std::optional<std::string>>
	{
		if (receiving_callback_ == nullptr)
		{
			return { false, "cannot complete ReceivingJob with null callback" };
		}

		auto data = get_data();
		if (data.empty())
		{
			return { false, "cannot complete ReceivingJob with null data" };
		}

		size_t index = 0;
		auto mode_data = Combiner::divide(data, index);
		auto array_data = Combiner::divide(data, index);
		if (mode_data.size() != 1)
		{
			return { false, "cannot complete ReceivingJob with null data mode" };
		}

		DataModes mode = (DataModes)mode_data[0];

		if (array_data.empty())
		{
			return { false, "cannot complete ReceivingJob with empty data" };
		}

		return receiving_callback_(mode, array_data);
	}
}