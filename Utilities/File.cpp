#include "File.h"

#include "Logger.h"
#include "Converter.h"
#include "Compressor.h"

#include <format>

#include <numeric>
#include <filesystem>

namespace Utilities
{
	File::File(void) : file_path_(""), openmode_(std::ios_base::openmode()) {}

	File::File(const std::string& path, const std::ios_base::openmode& mode) : File()
	{
		auto result = open(path, mode);
		if (!result)
		{
			Logger::handle().write(LogTypes::Error, result.error());
			return;
		}

		close();
	}

	File::File(const std::string& path, const std::ios_base::openmode& mode, const std::locale& locale) : File()
	{
		auto result = open(path, mode, locale);
		if (!result)
		{
			Logger::handle().write(LogTypes::Error, result.error());
			return;
		}

		close();
	}

	File::~File(void) { close(); }

	auto File::open(const std::string& path, const std::ios_base::openmode& mode) -> std::expected<void, std::string>
	{
		return open(path, mode, std::locale(""));
	}

	auto File::open(const std::string& path, const std::ios_base::openmode& mode, const std::locale& locale) -> std::expected<void, std::string>
	{
		file_path_ = path;
		openmode_ = mode;

		std::filesystem::path target_path(file_path_);
		if (target_path.parent_path().empty() != true)
		{
			std::filesystem::create_directories(target_path.parent_path());
		}

		stream_.open(file_path_, mode);

		if (!stream_.is_open())
		{
			if (!std::filesystem::exists(file_path_))
			{
				return std::unexpected(std::format("there is no file : {}", file_path_));
			}

			return std::unexpected(std::format("cannot open file : {}", file_path_));
		}

		stream_.imbue(locale);

		return {};
	}

	auto File::write_bytes(const uint8_t* bytes, size_t size) -> std::expected<void, std::string>
	{
		if (openmode_ & std::ios::in)
		{
			return std::unexpected(std::format("cannot write file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_));
		}

		if (!stream_.is_open())
		{
			return std::unexpected(std::format("cannot write file by unopened condition : {}", file_path_));
		}

		stream_.write((char*)bytes, (uint32_t)size);
		stream_.flush();

		return {};
	}

	auto File::write_bytes(const std::vector<uint8_t>& bytes) -> std::expected<void, std::string>
	{
		if (openmode_ & std::ios::in)
		{
			return std::unexpected(std::format("cannot write file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_));
		}

		if (!stream_.is_open())
		{
			return std::unexpected(std::format("cannot write file by unopened condition : {}", file_path_));
		}

		stream_.write((char*)bytes.data(), (uint32_t)bytes.size());
		stream_.flush();

		return {};
	}

	auto File::write_bytes(const std::deque<uint8_t>& bytes) -> std::expected<void, std::string>
	{
		if (openmode_ & std::ios::in)
		{
			return std::unexpected(std::format("cannot write file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_));
		}

		if (!stream_.is_open())
		{
			return std::unexpected(std::format("cannot write file by unopened condition : {}", file_path_));
		}

		std::vector<uint8_t> buffer(bytes.begin(), bytes.end());
		stream_.write((char*)buffer.data(), (uint32_t)buffer.size());
		stream_.flush();

		return {};
	}

	auto File::write_lines(const std::deque<std::string>& lines, bool append_newline) -> std::expected<void, std::string>
	{
		if (openmode_ & std::ios::in)
		{
			return std::unexpected(std::format("cannot write file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_));
		}

		if (!stream_.is_open())
		{
			return std::unexpected(std::format("cannot write file by unopened condition : {}", file_path_));
		}

		size_t total = 0;
		for (const auto& line : lines)
		{
			total += line.size();
			if (append_newline)
			{
				total += 1;
			}
		}

		std::string concatenated_message;
		concatenated_message.reserve(total);
		for (const auto& line : lines)
		{
			if (append_newline && !concatenated_message.empty())
			{
				concatenated_message += '\n';
			}
			concatenated_message += line;
		}
		stream_ << concatenated_message;
		if (append_newline)
		{
			stream_ << std::endl;
		}

		return {};
	}

	auto File::write_lines(const std::vector<std::string>& lines, bool append_newline) -> std::expected<void, std::string>
	{
		if (openmode_ & std::ios::in)
		{
			return std::unexpected(std::format("cannot write file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_));
		}

		if (!stream_.is_open())
		{
			return std::unexpected(std::format("cannot write file by unopened condition : {}", file_path_));
		}

		size_t total = 0;
		for (const auto& line : lines)
		{
			total += line.size();
			if (append_newline)
			{
				total += 1;
			}
		}

		std::string concatenated_message;
		concatenated_message.reserve(total);
		for (const auto& line : lines)
		{
			if (append_newline && !concatenated_message.empty())
			{
				concatenated_message += '\n';
			}
			concatenated_message += line;
		}
		stream_ << concatenated_message;
		if (append_newline)
		{
			stream_ << std::endl;
		}

		return {};
	}

	auto File::read_bytes(void) -> std::expected<std::vector<uint8_t>, std::string>
	{
		if (openmode_ & std::ios::out)
		{
			return std::unexpected(std::format("cannot read file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_));
		}

		if (!stream_.is_open())
		{
			return std::unexpected(std::format("cannot read file by unopened condition : {}", file_path_));
		}

		stream_.seekg(0, std::ios::beg);

		return std::vector<uint8_t>((std::istreambuf_iterator<char>(stream_)), std::istreambuf_iterator<char>());
	}

	auto File::read_bytes(size_t index, size_t size) -> std::expected<std::vector<uint8_t>, std::string>
	{
		if (openmode_ & std::ios::out)
		{
			return std::unexpected(std::format("cannot read file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_));
		}

		if (!stream_.is_open())
		{
			return std::unexpected(std::format("cannot read file by unopened condition : {}", file_path_));
		}

		stream_.seekg(index, std::ios::beg);
		if (stream_.fail())
		{
			return std::unexpected(std::format("failed to seek position: {} in file: {}", index, file_path_));
		}

		std::vector<uint8_t> buffer(size);
		stream_.read(reinterpret_cast<char*>(buffer.data()), size);
		buffer.resize(stream_.gcount());

		return buffer;
	}

	auto File::read_lines(bool include_new_line) -> std::expected<std::deque<std::string>, std::string>
	{
		if (openmode_ & std::ios::out)
		{
			return std::unexpected(std::format("cannot read file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_));
		}

		if (!stream_.is_open())
		{
			return std::unexpected(std::format("cannot read file by unopened condition : {}", file_path_));
		}

		stream_.seekg(0, std::ios::beg);

		std::string line;
		std::deque<std::string> file_lines;
		while (getline(stream_, line))
		{
			if (include_new_line)
			{
				line += "\n";
			}

			file_lines.push_back(line);
		}

		return file_lines;
	}

	void File::close(void)
	{
		if (!stream_.is_open())
		{
			return;
		}

		stream_.flush();
		stream_.close();

		file_path_ = "";
	}

	auto File::compression(const std::string& path, uint16_t block_bytes) -> std::expected<void, std::string>
	{
		File source;
		auto open_result = source.open(path, std::ios::in | std::ios::binary);
		if (!open_result)
		{
			return std::unexpected(open_result.error());
		}

		auto read_data = source.read_bytes();
		if (!read_data)
		{
			source.close();
			return std::unexpected(read_data.error());
		}
		source.close();

		auto compressed_bytes = Compressor::compression(read_data.value(), block_bytes);
		if (!compressed_bytes)
		{
			return std::unexpected(std::format("cannot compress file : {}", compressed_bytes.error()));
		}

		auto open_result2 = source.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!open_result2)
		{
			return std::unexpected(open_result2.error());
		}

		auto write_result = source.write_bytes(compressed_bytes.value());
		if (!write_result)
		{
			source.close();
			return std::unexpected(write_result.error());
		}
		source.close();

		return {};
	}

	auto File::decompression(const std::string& path, uint16_t block_bytes) -> std::expected<void, std::string>
	{
		File source;
		auto open_result = source.open(path, std::ios::in | std::ios::binary);
		if (!open_result)
		{
			return std::unexpected(open_result.error());
		}

		auto read_data = source.read_bytes();
		if (!read_data)
		{
			source.close();
			return std::unexpected(read_data.error());
		}
		source.close();

		auto decompressed_bytes = Compressor::decompression(read_data.value(), block_bytes);
		if (!decompressed_bytes)
		{
			return std::unexpected(std::format("cannot compress file : {}", decompressed_bytes.error()));
		}

		auto open_result2 = source.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!open_result2)
		{
			return std::unexpected(open_result2.error());
		}

		auto write_result = source.write_bytes(decompressed_bytes.value());
		if (!write_result)
		{
			source.close();
			return std::unexpected(write_result.error());
		}
		source.close();

		return {};
	}
}
