#include "ConnectionJob.h"

#include "Logger.h"
#include "Combiner.h"

#include "fmt/xchar.h"
#include "fmt/format.h"

using namespace Thread;
using namespace Utilities;

namespace Network
{
	ConnectionJob::ConnectionJob(
		const bool& condition,
		const bool& by_itself,
		const std::function<std::tuple<bool, std::optional<std::string>>(const bool&, const bool&)>& callback)
		: Job(JobPriorities::Normal, "ConnectionJob", true)
		, condition_(condition)
		, by_itself_(by_itself)
		, connection_callback_(callback)
	{
	}

	ConnectionJob::~ConnectionJob(void) {}

	auto ConnectionJob::working(void) -> std::tuple<bool, std::optional<std::string>>
	{
		if (connection_callback_ == nullptr)
		{
			return { false, "cannot complete ConnectionJob with null callback" };
		}

		return connection_callback_(condition_, by_itself_);
	}
}