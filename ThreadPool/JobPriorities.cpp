#include "JobPriorities.h"

#include <format>
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

		std::string joined_priorities;
		for (const auto& priority_string_value : priority_strings)
		{
			if (!joined_priorities.empty())
			{
				joined_priorities += ", ";
			}

			joined_priorities += priority_string_value;
		}

		return std::format("[ {} ]", joined_priorities);
	}
} // namespace Thread
