#pragma once

#include <string>
#include <vector>
#include <expected>

#include <cstdint>

namespace Utilities
{
	class Folder
	{
	public:
		Folder(void);
		~Folder(void);

		auto create_folder(const std::string& target_path) -> std::expected<void, std::string>;
		auto delete_folder(const std::string& target_path) -> std::expected<void, std::string>;
		auto get_folders(const std::string& target_path, bool search_sub_folder) -> std::expected<std::vector<std::string>, std::string>;
		auto get_files(const std::string& target_path, bool search_sub_folder, const std::vector<std::string>& extensions)
			-> std::expected<std::vector<std::string>, std::string>;

		static auto compression(const std::string& target_path,
								const std::string& source_path,
								bool search_sub_folder,
								const std::vector<std::string>& extensions,
								uint16_t block_bytes = 1024)
			-> std::expected<void, std::string>;
		static auto decompression(const std::string& target_path, const std::string& source_path, uint16_t block_bytes = 1024)
			-> std::expected<void, std::string>;
	};
}