#include "ArgumentParser.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include "boost/dll.hpp"

#include "Converter.h"

#include <regex>
#include <algorithm>
#include <filesystem>

namespace Utilities
{
	ArgumentParser::ArgumentParser(int32_t argc, char* argv[]) { arguments_ = parse(argc, argv); }

	ArgumentParser::ArgumentParser(int32_t argc, wchar_t* argv[]) { arguments_ = parse(argc, argv); }

	auto ArgumentParser::program_folder(void) const -> std::string { return program_folder_; }

	auto ArgumentParser::program_name(void) const -> std::string { return program_name_; }

	auto ArgumentParser::to_string(const std::string& key) const -> std::optional<std::string>
	{
		auto target = arguments_.find(key);
		if (target == arguments_.end())
		{
			return std::nullopt;
		}

		return target->second;
	}

	auto ArgumentParser::to_array(const std::string& key) const -> std::optional<std::vector<std::string>>
	{
		auto target = arguments_.find(key);
		if (target == arguments_.end())
		{
			return std::nullopt;
		}

		std::vector<std::string> result;
		std::regex reg(R"(\s*([^,]+)\s*)");
		std::smatch match;

		for (auto it = std::sregex_iterator(target->second.begin(), target->second.end(), reg); it != std::sregex_iterator(); ++it)
		{
			match = *it;
			result.push_back(match[1].str());
		}

		return result;
	}

	auto ArgumentParser::to_bool(const std::string& key) const -> std::optional<bool>
	{
		auto target = to_string(key);
		if (target == std::nullopt)
		{
			return std::nullopt;
		}

		auto temp = *target;
		transform(temp.begin(), temp.end(), temp.begin(), ::tolower);

		return temp.compare("true") == 0;
	}

	auto ArgumentParser::to_short(const std::string& key) const -> std::optional<int16_t>
	{
		auto target = to_string(key);
		if (target == std::nullopt)
		{
			return std::nullopt;
		}

		return (int16_t)atoi((*target).c_str());
	}

	auto ArgumentParser::to_ushort(const std::string& key) const -> std::optional<uint16_t>
	{
		auto target = to_string(key);
		if (target == std::nullopt)
		{
			return std::nullopt;
		}

		return (uint16_t)atoi((*target).c_str());
	}

	auto ArgumentParser::to_int(const std::string& key) const -> std::optional<int32_t>
	{
		auto target = to_string(key);
		if (target == std::nullopt)
		{
			return std::nullopt;
		}

		return (int32_t)atoi((*target).c_str());
	}

	auto ArgumentParser::to_uint(const std::string& key) const -> std::optional<uint32_t>
	{
		auto target = to_string(key);
		if (target == std::nullopt)
		{
			return std::nullopt;
		}

		return (uint32_t)atoi((*target).c_str());
	}

	auto ArgumentParser::to_long(const std::string& key) const -> std::optional<int64_t>
	{
		auto target = to_string(key);
		if (target == std::nullopt)
		{
			return std::nullopt;
		}

#ifdef _WIN32
		return (int64_t)atoll((*target).c_str());
#else
		return (int64_t)atol((*target).c_str());
#endif
	}

	auto ArgumentParser::to_ulong(const std::string& key) const -> std::optional<uint64_t>
	{
		auto target = to_string(key);
		if (target == std::nullopt)
		{
			return std::nullopt;
		}

#ifdef _WIN32
		return (uint64_t)atoll((*target).c_str());
#else
		return (uint64_t)atol((*target).c_str());
#endif
	}

	auto ArgumentParser::parse(int32_t argc, char* argv[]) -> std::map<std::string, std::string>
	{
		std::filesystem::path program_path(argv[0]);
		program_name_ = boost::dll::program_location().string();
		program_folder_ = fmt::format("{}/", boost::dll::program_location().parent_path().string());

		std::vector<std::string> arguments;
		for (int32_t index = 1; index < argc; ++index)
		{
			arguments.push_back(argv[index]);
		}

		return parse(arguments);
	}

	auto ArgumentParser::parse(int32_t argc, wchar_t* argv[]) -> std::map<std::string, std::string>
	{
		std::filesystem::path program_path(argv[0]);
		program_name_ = boost::dll::program_location().string();
		program_folder_ = fmt::format("{}/", boost::dll::program_location().parent_path().string());

		std::vector<std::string> arguments;
		for (int32_t index = 1; index < argc; ++index)
		{
			arguments.push_back(Converter::to_string(argv[index]));
		}

		return parse(arguments);
	}

	auto ArgumentParser::parse(const std::vector<std::string>& arguments) -> std::map<std::string, std::string>
	{
		std::map<std::string, std::string> result;

		size_t argc = arguments.size();
		std::string argument_id;
		for (size_t index = 0; index < argc; ++index)
		{
			argument_id = arguments[index];
			size_t offset = argument_id.find("--", 0);
			if (offset != 0)
			{
				continue;
			}

			if (argument_id.compare("--help") == 0)
			{
				result.insert({ argument_id, "display help" });
				continue;
			}

			if (argument_id.compare("--version") == 0)
			{
				result.insert({ argument_id, "display version" });
				continue;
			}

			if (index + 1 >= argc)
			{
				break;
			}

			auto target = result.find(argument_id);
			if (target == result.end())
			{
				result.insert({ argument_id, arguments[index + 1] });
				++index;

				continue;
			}

			target->second = arguments[index + 1];
			++index;
		}

		return result;
	}
}
