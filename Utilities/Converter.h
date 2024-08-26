#pragma once

#include <cstdint>
#include <locale>
#include <string>
#include <vector>

#define WSTRING_IS_U16STRING

namespace Utilities
{
	class Converter
	{
	public:
		static auto split(const std::string& source, const std::string& token) -> std::vector<std::string>;

		static auto replace(std::string& source, const std::string& token, const std::string& target) -> void;
		static auto replace2(const std::string& source, const std::string& token, const std::string& target) -> const std::string;

		static auto to_u32string(const std::u16string& value, std::locale target_locale = std::locale("")) -> std::u32string;
		static auto to_u32string(const std::wstring& value, std::locale target_locale = std::locale("")) -> std::u32string;
		static auto to_u32string(const std::string& value, std::locale target_locale = std::locale("")) -> std::u32string;

		static auto to_u16string(const std::u32string& value, std::locale target_locale = std::locale("")) -> std::u16string;
		static auto to_u16string(const std::wstring& value, std::locale target_locale = std::locale("")) -> std::u16string;
		static auto to_u16string(const std::string& value, std::locale target_locale = std::locale("")) -> std::u16string;

		static auto to_wstring(const std::u32string& value, std::locale target_locale = std::locale("")) -> std::wstring;
		static auto to_wstring(const std::u16string& value, std::locale target_locale = std::locale("")) -> std::wstring;
		static auto to_wstring(const std::string& value, std::locale target_locale = std::locale("")) -> std::wstring;

		static auto to_string(const std::u32string& value, std::locale target_locale = std::locale("")) -> std::string;
		static auto to_string(const std::u16string& value, std::locale target_locale = std::locale("")) -> std::string;
		static auto to_string(const std::wstring& value, std::locale target_locale = std::locale("")) -> std::string;

		static auto to_array(const std::u32string& value) -> std::vector<uint8_t>;
		static auto to_array(const std::u16string& value) -> std::vector<uint8_t>;
		static auto to_array(const std::wstring& value) -> std::vector<uint8_t>;
		static auto to_array(const std::string& value) -> std::vector<uint8_t>;

		static auto to_u32string(const std::vector<uint8_t>& value) -> std::u32string;
		static auto to_u16string(const std::vector<uint8_t>& value) -> std::u16string;
		static auto to_wstring(const std::vector<uint8_t>& value) -> std::wstring;
		static auto to_string(const std::vector<uint8_t>& value) -> std::string;

#ifdef USE_ENCRYPT_MODULE
		static auto from_base64(const std::string& value) -> std::vector<uint8_t>;
		static auto to_base64(const std::vector<uint8_t>& value) -> std::string;
#endif

#ifdef WSTRING_IS_U16STRING
	private:
		static auto convert(const std::u16string& value) -> std::wstring;
		static auto convert(const std::wstring& value) -> std::u16string;
#endif
	};
}
