#pragma once

#include "Job.h"
#include "DataModes.h"

#include <functional>

namespace Network
{
	class ConnectionJob : public Thread::Job
	{
	public:
		ConnectionJob(
			const bool& condition,
			const bool& by_itself,
			const std::function<std::tuple<bool, std::optional<std::string>>(const bool&, const bool&)>& callback);
		virtual ~ConnectionJob(void);

	private:
		auto working(void) -> std::tuple<bool, std::optional<std::string>> override;

	private:
		bool condition_;
		bool by_itself_;
		std::function<std::tuple<bool, std::optional<std::string>>(const bool&, const bool&)> connection_callback_;
	};
}