#include "ConnectionJob.h"

#include "Logger.h"
#include "Combiner.h"

#include <format>

using namespace Thread;
using namespace Utilities;

namespace Network
{
	ConnectionJob::ConnectionJob(
		bool condition,
		bool by_itself,
		const std::function<std::expected<void, std::string>(bool, bool)>& callback)
		: Job(JobPriorities::Normal, "ConnectionJob", true)
		, condition_(condition)
		, by_itself_(by_itself)
		, connection_callback_(callback)
	{
	}

	ConnectionJob::~ConnectionJob(void) {}

	auto ConnectionJob::working(void) -> std::expected<void, std::string>
	{
		if (connection_callback_ == nullptr)
		{
			return std::unexpected("cannot complete ConnectionJob with null callback");
		}

		return connection_callback_(condition_, by_itself_);
	}
}