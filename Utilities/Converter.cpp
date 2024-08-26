#include "Converter.h"

#include <codecvt>

#include "fmt/xchar.h"
#include "fmt/format.h"

#ifdef USE_ENCRYPT_MODULE
#ifdef TEST
#include "boost/archive/iterators/binary_from_base64.hpp"
#include "boost/archive/iterators/base64_from_binary.hpp"
#include "boost/archive/iterators/transform_width.hpp"
#include "boost/algorithm/string.hpp"
#else
#include "cryptopp/base64.h"

using namespace CryptoPP;
#endif
#endif

namespace Utilities
{
	auto Converter::split(const std::string& source, const std::string& token) -> std::vector<std::string>
	{
		if (source.empty() == true)
		{
			return {};
		}

		size_t offset = 0;
		size_t last_offset = 0;
		std::vector<std::string> result = {};
		std::string temp;

		while (true)
		{
			offset = source.find(token, last_offset);
			if (offset == std::string::npos)
			{
				break;
			}

			temp = source.substr(last_offset, offset - last_offset);
			if (!temp.empty())
			{
				result.push_back(temp);
			}

			last_offset = offset + token.size();
		}

		if (last_offset != 0 && last_offset != std::string::npos)
		{
			temp = source.substr(last_offset, offset - last_offset);
			if (!temp.empty())
			{
				result.push_back(temp);
			}
		}

		if (last_offset == 0)
		{
			return { source };
		}

		return result;
	}

	auto Converter::replace(std::string& source, const std::string& token, const std::string& target) -> void { source = replace2(source, token, target); }

	auto Converter::replace2(const std::string& source, const std::string& token, const std::string& target) -> const std::string
	{
		if (source.empty() == true)
		{
			return "";
		}

		size_t offset = 0;
		size_t last_offset = 0;
		fmt::memory_buffer result;
		result.clear();

		while (true)
		{
			offset = source.find(token, last_offset);
			if (offset == std::string::npos)
			{
				break;
			}

			fmt::format_to(back_inserter(result), "{}{}", source.substr(last_offset, offset - last_offset), target);

			last_offset = offset + token.size();
		}

		if (last_offset != 0 && last_offset != std::string::npos)
		{
			fmt::format_to(back_inserter(result), "{}", source.substr(last_offset, offset - last_offset));
		}

		if (last_offset == 0)
		{
			return source;
		}

		return std::string(result.begin(), result.end());
	}

	auto Converter::to_u32string(const std::u16string& value, std::locale target_locale) -> std::u32string
	{
		return to_u32string(to_string(value, target_locale), target_locale);
	}

	auto Converter::to_u32string(const std::wstring& value, std::locale target_locale) -> std::u32string
	{
#ifdef WSTRING_IS_U16STRING
		return to_u32string(to_string(convert(value), target_locale), target_locale);
#else
		return to_u32string(to_string(value, target_locale), target_locale);
#endif
	}

	auto Converter::to_u32string(const std::string& value, std::locale target_locale) -> std::u32string
	{
		if (value.empty())
		{
			return std::u32string();
		}

		typedef std::codecvt<char32_t, char, mbstate_t> codecvt_t;
		codecvt_t const& codecvt = std::use_facet<codecvt_t>(target_locale);

		mbstate_t state;
		memset(&state, 0, sizeof(mbstate_t));

		std::vector<char32_t> result(value.size() + 1);
		char const* in_text = value.data();
		char32_t* out_text = &result[0];
		codecvt_t::result condition = codecvt.in(state, in_text, in_text + value.size(), in_text, out_text, out_text + result.size(), out_text);

		return std::u32string(result.data());
	}

	auto Converter::to_u16string(const std::u32string& value, std::locale target_locale) -> std::u16string
	{
		return to_u16string(to_string(value, target_locale), target_locale);
	}

	auto Converter::to_u16string(const std::wstring& value, std::locale target_locale) -> std::u16string
	{
		return to_u16string(to_string(value, target_locale), target_locale);
	}

	auto Converter::to_u16string(const std::string& value, std::locale target_locale) -> std::u16string
	{
		if (value.empty())
		{
			return std::u16string();
		}

		typedef std::codecvt<char16_t, char, mbstate_t> codecvt_t;
		codecvt_t const& codecvt = std::use_facet<codecvt_t>(target_locale);

		mbstate_t state;
		memset(&state, 0, sizeof(mbstate_t));

		std::vector<char16_t> result(value.size() + 1);
		char const* in_text = value.data();
		char16_t* out_text = &result[0];
		codecvt_t::result condition = codecvt.in(state, in_text, in_text + value.size(), in_text, out_text, out_text + result.size(), out_text);

		return std::u16string(result.data());
	}

	auto Converter::to_wstring(const std::u32string& value, std::locale target_locale) -> std::wstring
	{
		return to_wstring(to_string(value, target_locale), target_locale);
	}

	auto Converter::to_wstring(const std::u16string& value, std::locale target_locale) -> std::wstring
	{
		return to_wstring(to_string(value, target_locale), target_locale);
	}

	auto Converter::to_wstring(const std::string& value, std::locale target_locale) -> std::wstring
	{
#ifdef WSTRING_IS_U16STRING
		return convert(to_u16string(value, target_locale));
#else
		if (value.empty())
		{
			return std::wstring();
		}

		typedef std::codecvt<wchar_t, char, mbstate_t> codecvt_t;
		codecvt_t const& codecvt = std::use_facet<codecvt_t>(target_locale);

		mbstate_t state;
		memset(&state, 0, sizeof(mbstate_t));

		std::vector<wchar_t> result(value.size() + 1);
		char const* in_text = value.data();
		wchar_t* out_text = &result[0];
		codecvt_t::result condition = codecvt.in(state, in_text, in_text + value.size(), in_text, out_text, out_text + result.size(), out_text);

		return std::wstring(result.data());
#endif
	}

	auto Converter::to_string(const std::u32string& value, std::locale target_locale) -> std::string
	{
		if (value.empty())
		{
			return std::string();
		}

		typedef std::codecvt<char32_t, char, mbstate_t> codecvt_t;
		codecvt_t const& codecvt = std::use_facet<codecvt_t>(target_locale);

		mbstate_t state = mbstate_t();

		std::vector<char> result((value.size() + 1) * codecvt.max_length());
		char32_t const* in_text = value.data();
		char* out_text = &result[0];

		codecvt_t::result condition = codecvt.out(state, in_text, in_text + value.size(), in_text, out_text, out_text + result.size(), out_text);

		return std::string(result.data());
	}

	auto Converter::to_string(const std::u16string& value, std::locale target_locale) -> std::string
	{
		if (value.empty())
		{
			return std::string();
		}

		typedef std::codecvt<char16_t, char, mbstate_t> codecvt_t;
		codecvt_t const& codecvt = std::use_facet<codecvt_t>(target_locale);

		mbstate_t state = mbstate_t();

		std::vector<char> result((value.size() + 1) * codecvt.max_length());
		char16_t const* in_text = value.data();
		char* out_text = &result[0];

		codecvt_t::result condition = codecvt.out(state, in_text, in_text + value.size(), in_text, out_text, out_text + result.size(), out_text);

		return std::string(result.data());
	}

	auto Converter::to_string(const std::wstring& value, std::locale target_locale) -> std::string
	{
#ifdef WSTRING_IS_U16STRING
		return to_string(convert(value));
#else
		if (value.empty())
		{
			return std::string();
		}

		typedef std::codecvt<wchar_t, char, mbstate_t> codecvt_t;
		codecvt_t const& codecvt = std::use_facet<codecvt_t>(target_locale);

		mbstate_t state = mbstate_t();

		std::vector<char> result((value.size() + 1) * codecvt.max_length());
		wchar_t const* in_text = value.data();
		char* out_text = &result[0];

		codecvt_t::result condition = codecvt.out(state, value.data(), value.data() + value.size(), in_text, &result[0], &result[0] + result.size(), out_text);

		return std::string(&result[0], out_text);
#endif
	}

	auto Converter::to_array(const std::u32string& value) -> std::vector<uint8_t> { return to_array(to_string(value)); }

	auto Converter::to_array(const std::u16string& value) -> std::vector<uint8_t> { return to_array(to_string(value)); }

	auto Converter::to_array(const std::wstring& value) -> std::vector<uint8_t>
	{
#ifdef WSTRING_IS_U16STRING
		return to_array(to_string(convert(value)));
#else
		return to_array(to_string(value));
#endif
	}

	auto Converter::to_array(const std::string& value) -> std::vector<uint8_t>
	{
		std::string temp = const_cast<std::string&>(value);

		return std::vector<uint8_t>(temp.data(), temp.data() + temp.size());
	}

	auto Converter::to_u32string(const std::vector<uint8_t>& value) -> std::u32string { return to_u32string(to_string(value)); }

	auto Converter::to_u16string(const std::vector<uint8_t>& value) -> std::u16string { return to_u16string(to_string(value)); }

	auto Converter::to_wstring(const std::vector<uint8_t>& value) -> std::wstring { return to_wstring(to_string(value)); }

	auto Converter::to_string(const std::vector<uint8_t>& value) -> std::string
	{
		if (value.empty())
		{
			return std::string();
		}

		// UTF-8 BOM
		if (value.size() >= 3 && value[0] == 0xef && value[1] == 0xbb && value[2] == 0xbf)
		{
			return std::string((char*)value.data() + 2, value.size() - 2);
		}

		// UTF-8 no BOM
		return std::string((char*)value.data(), value.size());
	}

#ifdef USE_ENCRYPT_MODULE
	auto Converter::from_base64(const std::string& value) -> std::vector<uint8_t>
	{
		if (value.empty())
		{
			return std::vector<uint8_t>();
		}

		std::string encoded;
		StringSource(value.data(), true, new Base64Decoder(new StringSink(encoded)));

		return std::vector<uint8_t>(encoded.data(), encoded.data() + encoded.size());
	}

	auto Converter::to_base64(const std::vector<uint8_t>& value) -> std::string
	{
		if (value.empty())
		{
			return std::string();
		}

		std::string decoded;
		StringSource(value.data(), value.size(), true, new Base64Encoder(new StringSink(decoded)));

		return decoded;
	}
#endif

#ifdef WSTRING_IS_U16STRING
	auto Converter::convert(const std::u16string& value) -> std::wstring { return std::wstring(value.begin(), value.end()); }

	auto Converter::convert(const std::wstring& value) -> std::u16string { return std::u16string(value.begin(), value.end()); }
#endif
}
