#include "JobPriorities.h"

#include "fmt/core.h"
#include "fmt/ranges.h"

namespace Thread
{
	auto priority_string(const JobPriorities& priority) -> const std::string
	{
		switch (priority)
		{
		case JobPriorities::Top:
			return "Top";
		case JobPriorities::High:
			return "High";
		case JobPriorities::Normal:
			return "Normal";
		case JobPriorities::Low:
			return "Low";
		case JobPriorities::LongTerm:
			return "LongTerm";
		default:
			return "Unknown";
		}
	}

	auto priority_string(const std::vector<JobPriorities>& priorities) -> const std::string
	{
		std::vector<std::string> priority_strings;
		for (const auto& priority : priorities)
		{
			priority_strings.push_back(priority_string(priority));
		}

		return fmt::format("[ {} ]", fmt::join(priority_strings, ", "));
	}
} // namespace Thread