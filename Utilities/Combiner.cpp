#include "Combiner.h"

#include <cstring>
#include <algorithm>

namespace Utilities
{
	auto Combiner::append(std::vector<uint8_t>& result, const std::vector<uint8_t>& source) -> void
	{
		size_t temp;
		const int32_t size = sizeof(size_t);
		char temp_size[size];

		temp = source.size();
		memcpy(temp_size, (char*)&temp, size);
		result.insert(result.end(), temp_size, temp_size + size);
		if (temp == 0)
		{
			return;
		}

		result.insert(result.end(), source.rbegin(), source.rend());
	}

	auto Combiner::divide(const std::vector<uint8_t>& source, size_t& index) -> std::vector<uint8_t>
	{
		if (source.empty())
		{
			return std::vector<uint8_t>();
		}

		size_t temp;
		const int32_t size = sizeof(size_t);

		if (source.size() < index + size)
		{
			return std::vector<uint8_t>();
		}

		memcpy(&temp, source.data() + index, size);
		index += size;

		if (temp == 0 || source.size() < index + temp)
		{
			return std::vector<uint8_t>();
		}

		std::vector<uint8_t> result;
		result.insert(result.end(), source.begin() + index, source.begin() + index + temp);
		std::reverse(result.begin(), result.end());
		index += temp;

		return result;
	}
}
