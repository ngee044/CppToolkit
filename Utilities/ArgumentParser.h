#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <optional>

namespace Utilities
{
	class ArgumentParser
	{
	public:
		ArgumentParser(int32_t argc, char* argv[]);
		ArgumentParser(int32_t argc, wchar_t* argv[]);

		auto program_name(void) const -> std::string;
		auto program_folder(void) const -> std::string;

		auto to_string(const std::string& key) const -> std::optional<std::string>;
		auto to_array(const std::string& key) const -> std::optional<std::vector<std::string>>;

		auto to_bool(const std::string& key) const -> std::optional<bool>;
		auto to_short(const std::string& key) const -> std::optional<int16_t>;
		auto to_ushort(const std::string& key) const -> std::optional<uint16_t>;
		auto to_int(const std::string& key) const -> std::optional<int32_t>;
		auto to_uint(const std::string& key) const -> std::optional<uint32_t>;
		auto to_long(const std::string& key) const -> std::optional<int64_t>;
		auto to_ulong(const std::string& key) const -> std::optional<uint64_t>;

	protected:
		auto parse(int32_t argc, char* argv[]) -> std::map<std::string, std::string>;
		auto parse(int32_t argc, wchar_t* argv[]) -> std::map<std::string, std::string>;

	private:
		auto parse(const std::vector<std::string>& arguments) -> std::map<std::string, std::string>;

	private:
		std::string program_name_;
		std::string program_folder_;
		std::map<std::string, std::string> arguments_;
	};
}
