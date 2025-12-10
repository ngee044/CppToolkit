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
		const auto [condition, message] = open(path, mode);
		if (!condition)
		{
			Logger::handle().write(LogTypes::Error, message.value());
			return;
		}

		close();
	}

	File::File(const std::string& path, const std::ios_base::openmode& mode, const std::locale& locale) : File()
	{
		const auto [condition, message] = open(path, mode, locale);
		if (!condition)
		{
			Logger::handle().write(LogTypes::Error, message.value());
			return;
		}

		close();
	}

	File::~File(void) { close(); }

	auto File::open(const std::string& path, const std::ios_base::openmode& mode) -> std::tuple<bool, std::optional<std::string>>
	{
		return open(path, mode, std::locale(""));
	}

	auto File::open(const std::string& path, const std::ios_base::openmode& mode, const std::locale& locale) -> std::tuple<bool, std::optional<std::string>>
	{
		file_path_ = path;

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
				return { false, std::format("there is no file : {}", file_path_) };
			}

			return { false, std::format("cannot open file : {}", file_path_) };
		}

		stream_.imbue(locale);

		return { true, std::nullopt };
	}

	auto File::write_bytes(const uint8_t* bytes, const size_t& size) -> std::tuple<bool, std::optional<std::string>>
	{
		if (openmode_ & std::ios::in)
		{
			return { false, std::format("cannot write file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_) };
		}

		if (!stream_.is_open())
		{
			return { false, std::format("cannot write file by unopened condition : {}", file_path_) };
		}

		stream_.write((char*)bytes, (uint32_t)size);
		stream_.flush();

		return { true, std::nullopt };
	}

	auto File::write_bytes(const std::vector<uint8_t>& bytes) -> std::tuple<bool, std::optional<std::string>>
	{
		if (openmode_ & std::ios::in)
		{
			return { false, std::format("cannot write file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_) };
		}

		if (!stream_.is_open())
		{
			return { false, std::format("cannot write file by unopened condition : {}", file_path_) };
		}

		stream_.write((char*)bytes.data(), (uint32_t)bytes.size());
		stream_.flush();

		return { true, std::nullopt };
	}

	auto File::write_bytes(const std::deque<uint8_t>& bytes) -> std::tuple<bool, std::optional<std::string>>
	{
		if (openmode_ & std::ios::in)
		{
			return { false, std::format("cannot write file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_) };
		}

		if (!stream_.is_open())
		{
			return { false, std::format("cannot write file by unopened condition : {}", file_path_) };
		}

		std::vector<uint8_t> buffer(bytes.begin(), bytes.end());
		stream_.write((char*)buffer.data(), (uint32_t)buffer.size());
		stream_.flush();

		return { true, std::nullopt };
	}

	auto File::write_lines(const std::deque<std::string>& lines, const bool& append_newline) -> std::tuple<bool, std::optional<std::string>>
	{
		if (openmode_ & std::ios::in)
		{
			return { false, std::format("cannot write file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_) };
		}

		if (!stream_.is_open())
		{
			return { false, std::format("cannot write file by unopened condition : {}", file_path_) };
		}

		std::string concatenated_message = std::accumulate(lines.begin(), lines.end(), std::string(),
														   [append_newline](const std::string& a, const std::string& b)
														   {
															   if (!append_newline)
															   {
																   return a + b;
															   }

															   return a + (a.empty() ? "" : "\n") + b;
														   });
		stream_ << concatenated_message;
		if (append_newline)
		{
			stream_ << std::endl;
		}

		return { true, std::nullopt };
	}

	auto File::write_lines(const std::vector<std::string>& lines, const bool& append_newline) -> std::tuple<bool, std::optional<std::string>>
	{
		if (openmode_ & std::ios::in)
		{
			return { false, std::format("cannot write file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_) };
		}

		if (!stream_.is_open())
		{
			return { false, std::format("cannot write file by unopened condition : {}", file_path_) };
		}

		std::string concatenated_message = std::accumulate(lines.begin(), lines.end(), std::string(),
														   [append_newline](const std::string& a, const std::string& b)
														   {
															   if (!append_newline)
															   {
																   return a + b;
															   }

															   return a + (a.empty() ? "" : "\n") + b;
														   });
		stream_ << concatenated_message;
		if (append_newline)
		{
			stream_ << std::endl;
		}

		return { true, std::nullopt };
	}

	auto File::read_bytes(void) -> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>
	{
		if (openmode_ & std::ios::out)
		{
			return { std::nullopt, std::format("cannot read file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_) };
		}

		if (!stream_.is_open())
		{
			return { std::nullopt, std::format("cannot read file by unopened condition : {}", file_path_) };
		}

		stream_.seekg(0, std::ios::beg);

		return { std::vector<uint8_t>((std::istreambuf_iterator<char>(stream_)), std::istreambuf_iterator<char>()), std::nullopt };
	}

	auto File::read_bytes(const size_t& index, const size_t& size) -> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>
	{
		if (openmode_ & std::ios::out)
		{
			return { std::nullopt, std::format("cannot read file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_) };
		}

		if (!stream_.is_open())
		{
			return { std::nullopt, std::format("cannot read file by unopened condition : {}", file_path_) };
		}

		stream_.seekg(index, std::ios::beg);
		if (stream_.fail())
		{
			return { std::nullopt, std::format("failed to seek position: {} in file: {}", index, file_path_) };
		}

		std::vector<uint8_t> buffer(size);
		stream_.read(reinterpret_cast<char*>(buffer.data()), size);
		buffer.resize(stream_.gcount());

		return { buffer, std::nullopt };
	}

	auto File::read_lines(const bool& include_new_line) -> std::tuple<std::optional<std::deque<std::string>>, std::optional<std::string>>
	{
		if (openmode_ & std::ios::out)
		{
			return { std::nullopt, std::format("cannot read file by wrong openmode : {} -> {}", static_cast<int>(openmode_), file_path_) };
		}

		if (!stream_.is_open())
		{
			return { std::nullopt, std::format("cannot read file by unopened condition : {}", file_path_) };
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

		return { file_lines, std::nullopt };
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

	auto File::compression(const std::string& path, const uint16_t& block_bytes) -> std::tuple<bool, std::optional<std::string>>
	{
		File source;
		auto [open_condition, open_message] = source.open(path, std::ios::in | std::ios::binary);
		if (!open_condition)
		{
			return { open_condition, open_message };
		}

		auto [read_data, read_message] = source.read_bytes();
		if (read_data == std::nullopt)
		{
			source.close();
			return { false, read_message };
		}
		source.close();

		auto [compressed_bytes, compressed_message] = Compressor::compression(read_data.value(), block_bytes);
		if (compressed_bytes == std::nullopt)
		{
			return { false, std::format("cannot compress file : {}", compressed_message.value()) };
		}

		auto [open_condition2, open_message2] = source.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!open_condition2)
		{
			return { open_condition2, open_message2 };
		}

		auto [write_condition, write_message] = source.write_bytes(compressed_bytes.value());
		if (!write_condition)
		{
			source.close();
			return { write_condition, write_message };
		}
		source.close();

		return { true, std::nullopt };
	}

	auto File::decompression(const std::string& path, const uint16_t& block_bytes) -> std::tuple<bool, std::optional<std::string>>
	{
		File source;
		auto [open_condition, open_message] = source.open(path, std::ios::in | std::ios::binary);
		if (!open_condition)
		{
			return { open_condition, open_message };
		}

		auto [read_data, read_message] = source.read_bytes();
		if (read_data == std::nullopt)
		{
			source.close();
			return { false, read_message };
		}
		source.close();

		auto [decompressed_bytes, decompressed_message] = Compressor::decompression(read_data.value(), block_bytes);
		if (decompressed_bytes == std::nullopt)
		{
			return { false, std::format("cannot compress file : {}", decompressed_message.value()) };
		}

		auto [open_condition2, open_message2] = source.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!open_condition2)
		{
			return { open_condition2, open_message2 };
		}

		auto [write_condition, write_message] = source.write_bytes(decompressed_bytes.value());
		if (!write_condition)
		{
			source.close();
			return { write_condition, write_message };
		}
		source.close();

		return { true, std::nullopt };
	}
}
