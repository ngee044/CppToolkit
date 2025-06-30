#pragma once

#include <string>
#include <vector>
#include <stdint.h>

namespace Thread
{
	enum class JobPriorities : uint8_t
	{
		Top,
		High,
		Normal,
		Low,
		LongTerm,
	};

	auto priority_string(const JobPriorities& priority) -> const std::string;
	auto priority_string(const std::vector<JobPriorities>& priorities) -> const std::string;
}