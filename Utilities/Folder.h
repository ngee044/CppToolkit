#pragma once

#include <tuple>
#include <string>
#include <vector>
#include <optional>

#include <cstdint>

namespace Utilities
{
	class Folder
	{
	public:
		Folder(void);
		~Folder(void);

		auto create_folder(const std::string& target_path) -> std::tuple<bool, std::optional<std::string>>;
		auto delete_folder(const std::string& target_path) -> std::tuple<bool, std::optional<std::string>>;
		auto get_folders(const std::string& target_path, const bool& search_sub_folder) -> std::tuple<std::optional<std::vector<std::string>>, std::optional<std::string>>;
		auto get_files(const std::string& target_path, const bool& search_sub_folder, const std::vector<std::string>& extensions)
			-> std::tuple<std::optional<std::vector<std::string>>, std::optional<std::string>>;

		static auto compression(const std::string& target_path,
								const std::string& source_path,
								const bool& search_sub_folder,
								const std::vector<std::string>& extensions,
								const uint16_t& block_bytes = 1024)
			-> std::tuple<bool, std::optional<std::string>>;
		static auto decompression(const std::string& target_path, const std::string& source_path, const uint16_t& block_bytes = 1024)
			-> std::tuple<bool, std::optional<std::string>>;
	};
}