#include "Compressor.h"

#include "Logger.h"

#include "lz4.h"

#include <format>

namespace Utilities
{
	auto Compressor::compression(const std::vector<uint8_t>& original_data, const uint16_t& block_bytes)
		-> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>
	{
		if (original_data.empty())
		{
			return { std::nullopt, "the data field is empty." };
		}

		LZ4_stream_t lz4Stream_body;
		size_t read_index = 0;
		int32_t source_buffer_index = 0;

		std::vector<std::vector<char>> source_buffer;
		source_buffer.push_back(std::vector<char>());
		source_buffer.push_back(std::vector<char>());
		source_buffer[0].reserve(block_bytes);
		source_buffer[1].reserve(block_bytes);

		int32_t original_size = 0;
		int32_t compress_size = LZ4_COMPRESSBOUND(block_bytes);
		std::vector<char> compress_buffer;
		compress_buffer.reserve(compress_size);
		std::vector<uint8_t> compressed_data;

		char original_size_data[4];
		char compress_size_data[4];

		LZ4_resetStream(&lz4Stream_body);

		while (true)
		{
			char* const source_buffer_pointer = source_buffer[source_buffer_index].data();
			memset(source_buffer_pointer, 0, sizeof(char) * block_bytes);

			const size_t inpBytes = ((original_data.size() - read_index) > block_bytes)
										? block_bytes
										: (original_data.size() - read_index);
			if (0 == inpBytes)
			{
				break;
			}

			original_size = inpBytes;

			memcpy(source_buffer_pointer, original_data.data() + read_index, sizeof(char) * inpBytes);

			{
				char* const compress_buffer_pointer = compress_buffer.data();
				memset(compress_buffer_pointer, 0, sizeof(char) * compress_size);

				const int32_t compressed_size
					= LZ4_compress_fast_continue(&lz4Stream_body, (const char*)source_buffer_pointer,
												 (char*)compress_buffer_pointer, (int32_t)inpBytes, compress_size, 1);
				if (compressed_size <= 0)
				{
					break;
				}

				memcpy(original_size_data, &original_size, sizeof(int32_t));
				memcpy(compress_size_data, &compressed_size, sizeof(int32_t));

				compressed_data.insert(compressed_data.end(), original_size_data, original_size_data + sizeof(int32_t));
				compressed_data.insert(compressed_data.end(), compress_size_data, compress_size_data + sizeof(int32_t));
				compressed_data.insert(compressed_data.end(), compress_buffer_pointer,
									   compress_buffer_pointer + compressed_size);
			}

			read_index += inpBytes;
			source_buffer_index = (source_buffer_index + 1) % 2;
		}

		source_buffer[0].clear();
		source_buffer[1].clear();
		source_buffer.clear();

		if (compressed_data.empty())
		{
			return { std::nullopt, "cannot compress data" };
		}

		return { compressed_data,
				 std::format("compressing(buffer {}): ({} -> {} : {:.2f} %)", block_bytes, original_data.size(),
							 compressed_data.size(),
							 (((double)compressed_data.size() / (double)original_data.size()) * 100)) };
	}

	auto Compressor::decompression(const std::vector<uint8_t>& compressed_data, const uint16_t& block_bytes)
		-> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>
	{
		if (compressed_data.empty())
		{
			return { std::nullopt, "the data field is empty." };
		}

		LZ4_streamDecode_t lz4StreamDecode_body;

		int32_t read_index = 0;
		int32_t original_size = 0;
		int32_t compressed_size = 0;

		int32_t target_buffer_index = 0;

		std::vector<std::vector<char>> target_buffer;
		target_buffer.push_back(std::vector<char>());
		target_buffer.push_back(std::vector<char>());
		target_buffer[0].reserve(block_bytes);
		target_buffer[1].reserve(block_bytes);

		int32_t compress_size = LZ4_COMPRESSBOUND(block_bytes);
		std::vector<char> compress_buffer;
		compress_buffer.reserve(compress_size);
		std::vector<uint8_t> decompressed_data;

		LZ4_setStreamDecode(&lz4StreamDecode_body, NULL, 0);

		while (true)
		{
			char* const compress_buffer_pointer = compress_buffer.data();

			memset(compress_buffer_pointer, 0, sizeof(char) * compress_size);
			if ((compressed_data.size() - read_index) < 1)
			{
				break;
			}

			memcpy(&original_size, compressed_data.data() + read_index, sizeof(int32_t));
			if (0 >= original_size)
			{
				break;
			}
			read_index += sizeof(int32_t);

			memcpy(&compressed_size, compressed_data.data() + read_index, sizeof(int32_t));
			if (0 >= compressed_size)
			{
				break;
			}
			read_index += sizeof(int32_t);

			memcpy(compress_buffer_pointer, compressed_data.data() + read_index, sizeof(char) * compressed_size);
			read_index += compressed_size;

			char* const target_buffer_pointer = target_buffer[target_buffer_index].data();
			const int32_t decompressed_size
				= LZ4_decompress_safe_continue(&lz4StreamDecode_body, (const char*)compress_buffer_pointer,
											   (char*)target_buffer_pointer, compressed_size, block_bytes);
			if (decompressed_size <= 0)
			{
				break;
			}

			decompressed_data.insert(decompressed_data.end(), target_buffer_pointer,
									 target_buffer_pointer + decompressed_size);

			target_buffer_index = (target_buffer_index + 1) % 2;
		}

		target_buffer[0].clear();
		target_buffer[1].clear();
		target_buffer.clear();

		if (decompressed_data.empty())
		{
			return { std::nullopt, "cannot decompress data" };
		}

		return { decompressed_data,
				 std::format("decompressing(buffer {}): ({} -> {} : {:.2f} %)", block_bytes, compressed_data.size(),
							 decompressed_data.size(),
							 (((double)compressed_data.size() / (double)decompressed_data.size()) * 100)) };
	}
}
