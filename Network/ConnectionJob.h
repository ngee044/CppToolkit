#pragma once

#include "Job.h"
#include "DataModes.h"

#include <expected>
#include <functional>

namespace Network
{
	class ConnectionJob : public Thread::Job
	{
	public:
		ConnectionJob(
			bool condition,
			bool by_itself,
			const std::function<std::expected<void, std::string>(bool, bool)>& callback);
		virtual ~ConnectionJob(void);

	private:
		auto working(void) -> std::expected<void, std::string> override;

	private:
		bool condition_;
		bool by_itself_;
		std::function<std::expected<void, std::string>(bool, bool)> connection_callback_;
	};
}