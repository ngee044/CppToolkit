#pragma once

#include <cstdint>
#include <locale>
#include <fstream>
#include <deque>
#include <string>
#include <vector>
#include <expected>

namespace Utilities
{
	class File
	{
	public:
		File(void);
		File(const std::string& path, const std::ios_base::openmode& mode);
		File(const std::string& path, const std::ios_base::openmode& mode, const std::locale& locale);
		~File(void);

		auto open(const std::string& path, const std::ios_base::openmode& mode) -> std::expected<void, std::string>;
		auto open(const std::string& path, const std::ios_base::openmode& mode, const std::locale& locale)
			-> std::expected<void, std::string>;

		auto write_bytes(const uint8_t* bytes, size_t size) -> std::expected<void, std::string>;
		auto write_bytes(const std::deque<uint8_t>& bytes) -> std::expected<void, std::string>;
		auto write_bytes(const std::vector<uint8_t>& bytes) -> std::expected<void, std::string>;
		auto write_lines(const std::deque<std::string>& lines, bool append_newline = false)
			-> std::expected<void, std::string>;
		auto write_lines(const std::vector<std::string>& lines, bool append_newline = false)
			-> std::expected<void, std::string>;
		auto read_bytes(void) -> std::expected<std::vector<uint8_t>, std::string>;
		auto read_bytes(size_t index, size_t size) -> std::expected<std::vector<uint8_t>, std::string>;
		auto read_lines(bool include_new_line = true)
			-> std::expected<std::deque<std::string>, std::string>;
		void close(void);

		static auto compression(const std::string& path, uint16_t block_bytes = 1024) -> std::expected<void, std::string>;
		static auto decompression(const std::string& path, uint16_t block_bytes = 1024) -> std::expected<void, std::string>;

	private:
		std::fstream stream_;
		std::string file_path_;
		std::ios_base::openmode openmode_;
	};
}
