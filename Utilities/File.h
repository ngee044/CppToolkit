#pragma once

#include <tuple>
#include <cstdint>
#include <locale>
#include <fstream>
#include <deque>
#include <string>
#include <vector>
#include <optional>

namespace Utilities
{
	class File
	{
	public:
		File(void);
		File(const std::string& path, const std::ios_base::openmode& mode);
		File(const std::string& path, const std::ios_base::openmode& mode, const std::locale& locale);
		~File(void);

		auto open(const std::string& path, const std::ios_base::openmode& mode) -> std::tuple<bool, std::optional<std::string>>;
		auto open(const std::string& path, const std::ios_base::openmode& mode, const std::locale& locale)
			-> std::tuple<bool, std::optional<std::string>>;

		auto write_bytes(const uint8_t* bytes, const size_t& size) -> std::tuple<bool, std::optional<std::string>>;
		auto write_bytes(const std::deque<uint8_t>& bytes) -> std::tuple<bool, std::optional<std::string>>;
		auto write_bytes(const std::vector<uint8_t>& bytes) -> std::tuple<bool, std::optional<std::string>>;
		auto write_lines(const std::deque<std::string>& lines, const bool& append_newline = false)
			-> std::tuple<bool, std::optional<std::string>>;
		auto write_lines(const std::vector<std::string>& lines, const bool& append_newline = false)
			-> std::tuple<bool, std::optional<std::string>>;
		auto read_bytes(void) -> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>;
		auto read_bytes(const size_t& index, const size_t& size) -> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>;
		auto read_lines(const bool& include_new_line = true)
			-> std::tuple<std::optional<std::deque<std::string>>, std::optional<std::string>>;
		void close(void);
		
		static auto compression(const std::string& path, const uint16_t& block_bytes = 1024) -> std::tuple<bool, std::optional<std::string>>;
		static auto decompression(const std::string& path, const uint16_t& block_bytes = 1024) -> std::tuple<bool, std::optional<std::string>>;

	private:
		std::fstream stream_;
		std::string file_path_;
		std::ios_base::openmode openmode_;
	};
}
