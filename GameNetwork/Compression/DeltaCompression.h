#pragma once

#include "GameNetworkConstants.h"
#include "NetworkPrediction.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <bitset>
#include <cstring>
#include <mutex>

namespace GameNetwork
{
	namespace Compression
	{
		// Delta packet structure
		struct DeltaPacket
		{
			uint32_t base_sequence;
			uint32_t current_sequence;
			std::vector<uint8_t> delta_data;
			std::bitset<1024> changed_fields; // Track which fields changed
		};

		// Field descriptor for serialization
		struct FieldDescriptor
		{
			std::string name;
			size_t offset;
			size_t size;
			bool is_floating_point;
			float quantization_precision;
		};

		class DeltaCompression
		{
		public:
			DeltaCompression();
			virtual ~DeltaCompression() = default;

			template<typename T>
			auto register_state_type() -> void;

			template<typename T>
			auto create_delta_snapshot(const T& old_state, const T& new_state) 
				-> DeltaPacket;

				template<typename T>
			auto apply_delta(const T& base_state, const DeltaPacket& delta) -> T;

			auto create_delta_snapshot(const Prediction::EntityState& old_state, const Prediction::EntityState& new_state) 
				-> DeltaPacket;
            
			auto apply_delta(const Prediction::EntityState& base, const DeltaPacket& delta) 
				-> Prediction::EntityState;

			struct CompressionSettings
			{
				float position_precision = 0.01f; // 1cm precision
				float rotation_precision = 0.1f;  // 0.1 degree precision
				float velocity_precision = 0.1f;
				bool use_huffman_encoding = true;
				bool use_run_length_encoding = true;
			};

			auto configure(const CompressionSettings& settings) -> void;

			// State tracking
			auto store_baseline(uint32_t sequence, const void* state, size_t size) -> void;
			auto get_baseline(uint32_t sequence) -> std::optional<std::vector<uint8_t>>;
			auto cleanup_old_baselines(uint32_t oldest_sequence) -> void;

			// Statistics
			struct CompressionStats
			{
				uint64_t deltas_created;
				uint64_t deltas_applied;
				float average_compression_ratio;
				size_t total_bytes_saved;
			};

			auto get_statistics() const -> CompressionStats;
		private:
			// Compression helpers
			auto compress_float(float value, float precision) -> uint32_t;
			auto decompress_float(uint32_t compressed, float precision) -> float;
			auto calculate_field_delta(const void* old_data, const void* new_data, const FieldDescriptor& field) -> std::vector<uint8_t>;

			// Vector3 and quaternion helpers
			bool position_equals(const glm::vec3& a, const glm::vec3& b, float precision);
			bool rotation_equals(const glm::quat& a, const glm::quat& b, float precision);
			bool vector_equals(const glm::vec3& a, const glm::vec3& b, float precision);
			void append_compressed_vector3(std::vector<uint8_t>& data, const glm::vec3& vec, float precision);
			void append_compressed_quaternion(std::vector<uint8_t>& data, const glm::quat& quat);
			glm::vec3 extract_compressed_vector3(const std::vector<uint8_t>& data, size_t& offset, float precision);
			glm::quat extract_compressed_quaternion(const std::vector<uint8_t>& data, size_t& offset);

			// Huffman encoding
			auto huffman_encode(const std::vector<uint8_t>& data) -> std::vector<uint8_t>;
			auto huffman_decode(const std::vector<uint8_t>& data) -> std::vector<uint8_t>;

			// Run-length encoding
			auto run_length_encode(const std::vector<uint8_t>& data) -> std::vector<uint8_t>;
			auto run_length_decode(const std::vector<uint8_t>& data) -> std::vector<uint8_t>;

		private:
			CompressionSettings settings_;
			mutable std::mutex mutex_;

			// Type information
			std::unordered_map<std::string, std::vector<FieldDescriptor>> type_descriptors_;

			// Baseline storage
			std::unordered_map<uint32_t, std::vector<uint8_t>> baselines_;

			// Statistics
			CompressionStats stats_;
		};
	}
}