#include "Folder.h"

#include "File.h"
#include "Combiner.h"
#include "Converter.h"

#include <algorithm>
#include <filesystem>

namespace Utilities
{
	Folder::Folder(void) {}

	Folder::~Folder(void) { }

	auto Folder::create_folder(const std::string& target_path) -> std::tuple<bool, std::optional<std::string>>
	{
		if (target_path.empty())
		{
			return { false, "target path is empty" };
		}

		std::filesystem::path target(target_path);
		if (std::filesystem::exists(target))
		{
			return { false, "target folder already exists" };
		}

		std::error_code error_code;
		if (!std::filesystem::create_directories(target, error_code))
		{
			return { false, error_code.message() };
		}

		return { true, std::nullopt };
	}

	auto Folder::delete_folder(const std::string& target_path) -> std::tuple<bool, std::optional<std::string>>
	{
		if (target_path.empty())
		{
			return { false, "target path is empty" };
		}

		std::filesystem::path target(target_path);
		if (!std::filesystem::exists(target))
		{
			return { false, "there is no target folder" };
		}

		std::error_code error_code;
		auto deleted_count = std::filesystem::remove_all(target, error_code);
		if (deleted_count == 0)
		{
			return { false, error_code.message() };
		}

		return { true, std::nullopt };
	}

	auto Folder::get_folders(const std::string& target_path, const bool& search_sub_folder) -> std::tuple<std::optional<std::vector<std::string>>, std::optional<std::string>>
	{
		std::vector<std::string> result;

		if (target_path.empty())
		{
			return { std::nullopt, "target path is empty" };
		}

		std::filesystem::path target(target_path);
		if (!std::filesystem::exists(target))
		{
			return { std::nullopt, "there is no target folder" };
		}

		std::filesystem::directory_iterator iterator(target);
		for (const auto& entry : iterator)
		{
			if (!std::filesystem::is_directory(entry))
			{
				continue;
			}

			result.push_back(entry.path().string());

			if (search_sub_folder)
			{
				auto [sub_folders, error_message] = get_folders(entry.path().string(), search_sub_folder);
				if (!sub_folders.has_value())
				{
					continue;
				}

				auto target_folders = sub_folders.value();
				result.insert(result.end(), target_folders.begin(), target_folders.end());
			}
		}

		return { result, std::nullopt };
	}

	auto Folder::get_files(const std::string& target_path, const bool& search_sub_folder, const std::vector<std::string>& extensions) -> std::tuple<std::optional<std::vector<std::string>>, std::optional<std::string>>
	{
		std::vector<std::string> result;

		if (target_path.empty())
		{
			return { std::nullopt, "target path is empty" };
		}

		std::filesystem::path target(target_path);
		if (!std::filesystem::exists(target))
		{
			return { std::nullopt, "there is no target folder" };
		}

		std::filesystem::directory_iterator iterator(target);
		for (const auto& entry : iterator)
		{
			if (std::filesystem::is_directory(entry) && search_sub_folder)
			{
				auto [sub_folders, error_message] = get_files(entry.path().string(), search_sub_folder, extensions);
				if (!sub_folders.has_value())
				{
					continue;
				}

				auto target_folders = sub_folders.value();
				result.insert(result.end(), target_folders.begin(), target_folders.end());

				continue;
			}

			if (!std::filesystem::is_regular_file(entry))
			{
				continue;
			}

			if (extensions.empty() || std::find(extensions.begin(), extensions.end(), entry.path().extension().string()) != extensions.end())
			{
				result.push_back(entry.path().string());
			}
		}

		return { result, std::nullopt };
	}

	
	auto Folder::compression(const std::string& target_path,
							 const std::string& source_path,
							 const bool& search_sub_folder,
							 const std::vector<std::string>& extensions,
							 const uint16_t& block_bytes)
		-> std::tuple<bool, std::optional<std::string>>
	{
		Folder folder;
		auto [search_files, search_message] = folder.get_files(source_path, search_sub_folder, extensions);
		if (!search_files.has_value())
		{
			return { false, search_message };
		}

		File target_file;
		auto [open_condition2, open_message2] = target_file.open(target_path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!open_condition2)
		{
			return { open_condition2, open_message2 };
		}

		for (const auto& search_file : search_files.value())
		{
			File source;
			auto [open_condition, open_message] = source.open(search_file, std::ios::in | std::ios::binary);
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

			std::vector<uint8_t> source_data;
			std::filesystem::path new_path(search_file);
			Combiner::append(source_data, Converter::to_array(new_path.lexically_relative(source_path).string()));
			Combiner::append(source_data, read_data.value());
			std::reverse(source_data.begin(), source_data.end());

			size_t temp;
			const int32_t size = sizeof(size_t);
			temp = source_data.size();

			auto [write_condition, write_message] = target_file.write_bytes((uint8_t*)&temp, size);
			if (!write_condition)
			{
				target_file.close();
				return { write_condition, write_message };
			}

			auto [write_condition2, write_message2] = target_file.write_bytes(source_data);
			if (!write_condition2)
			{
				target_file.close();
				return { write_condition2, write_message2 };
			}
		}
		
		target_file.close();

		return { true, std::nullopt };
	}

	auto Folder::decompression(const std::string& target_path, const std::string& source_path, const uint16_t& block_bytes)
		-> std::tuple<bool, std::optional<std::string>>
	{
		File source;
		auto [open_condition, open_message] = source.open(source_path, std::ios::in | std::ios::binary);
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

		Folder folder;
		folder.create_folder(target_path);

		auto& source_data = read_data.value();

		size_t index = 0;
		while (true)
		{
			auto parsed_data = Combiner::divide(source_data, index);
			if (parsed_data.empty())
			{
				break;
			}

			size_t sub_index = 0;
			auto file_path = Converter::to_string(Combiner::divide(parsed_data, sub_index));
			auto file_data = Combiner::divide(parsed_data, sub_index);

			std::filesystem::path new_path(target_path);
			new_path.append(file_path);

			File new_file;
			auto [open_condition2, open_message2] = new_file.open(new_path.string(), std::ios::out | std::ios::binary | std::ios::trunc);
			if (!open_condition2)
			{
				return { open_condition2, open_message2 };
			}

			auto [write_condition, write_message] = new_file.write_bytes(file_data);
			if (!write_condition)
			{
				source.close();
				return { write_condition, write_message };
			}
			source.close();
		}

		return { true, std::nullopt };
	}
}
