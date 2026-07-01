#pragma once

#ifdef USE_ENCRYPT_MODULE

#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <expected>

namespace Utilities
{
	class Encryptor
	{
	public:
		static auto create_key(void) -> std::pair<std::string, std::string>;
		static auto encryption(const std::vector<uint8_t>& original_data, const std::string& key, const std::string& iv)
			-> std::expected<std::vector<uint8_t>, std::string>;
		static auto
		decryption(const std::vector<uint8_t>& encrypted_data, const std::string& key, const std::string& iv)
			-> std::expected<std::vector<uint8_t>, std::string>;
	};
}

#endif
