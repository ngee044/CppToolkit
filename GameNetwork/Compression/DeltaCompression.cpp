#include "DeltaCompression.h"
#include <Logger.h>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <array>

namespace GameNetwork
{
    namespace Compression
    {
        DeltaCompression::DeltaCompression()
            : stats_{0, 0, 0.0f, 0}
        {
            // Initialize default settings
            configure(CompressionSettings{});
        }

        auto DeltaCompression::configure(const CompressionSettings& settings) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            settings_ = settings;
        }

        auto DeltaCompression::create_delta_snapshot(const Prediction::EntityState& old_state,
                                                     const Prediction::EntityState& new_state)
            -> DeltaPacket
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            DeltaPacket packet;
            packet.base_sequence = old_state.sequence_number;
            packet.current_sequence = new_state.sequence_number;
            
            // Track changed fields
            size_t field_index = 0;

            // Check entity ID (should rarely change)
            if (old_state.entity_id != new_state.entity_id)
            {
                packet.changed_fields.set(field_index);
                auto id_bytes = reinterpret_cast<const uint8_t*>(&new_state.entity_id);
                packet.delta_data.insert(packet.delta_data.end(), id_bytes, id_bytes + sizeof(uint32_t));
            }
            field_index++;

            // Check position with precision
            if (!position_equals(old_state.position, new_state.position, settings_.position_precision))
            {
                packet.changed_fields.set(field_index);
                // Compress position delta
                glm::vec3 delta = new_state.position - old_state.position;
                append_compressed_vector3(packet.delta_data, delta, settings_.position_precision);
            }
            field_index++;

            // Check rotation with precision
            if (!rotation_equals(old_state.rotation, new_state.rotation, settings_.rotation_precision))
            {
                packet.changed_fields.set(field_index);
                // Store full rotation (quaternion compression)
                append_compressed_quaternion(packet.delta_data, new_state.rotation);
            }
            field_index++;

            // Check velocity
            if (!vector_equals(old_state.velocity, new_state.velocity, settings_.velocity_precision))
            {
                packet.changed_fields.set(field_index);
                append_compressed_vector3(packet.delta_data, new_state.velocity, settings_.velocity_precision);
            }
            field_index++;

            // Apply additional compression if enabled
            if (settings_.use_run_length_encoding && packet.delta_data.size() > 10)
            {
                packet.delta_data = run_length_encode(packet.delta_data);
            }
            
            if (settings_.use_huffman_encoding && packet.delta_data.size() > 20)
            {
                packet.delta_data = huffman_encode(packet.delta_data);
            }

            // Update statistics
            stats_.deltas_created++;
            size_t original_size = sizeof(Prediction::EntityState);
            size_t delta_size = packet.delta_data.size() + sizeof(packet.changed_fields);
            if (original_size > delta_size)
            {
                stats_.total_bytes_saved += (original_size - delta_size);
            }
            stats_.average_compression_ratio = 
                (stats_.average_compression_ratio * (stats_.deltas_created - 1) + 
                 static_cast<float>(delta_size) / original_size) / stats_.deltas_created;

            return packet;
        }

        auto DeltaCompression::apply_delta(const Prediction::EntityState& base_state,
                                          const DeltaPacket& delta)
            -> Prediction::EntityState
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            Prediction::EntityState result = base_state;
            std::vector<uint8_t> decompressed_data = delta.delta_data;

            // Decompress if needed
            if (settings_.use_huffman_encoding && decompressed_data.size() > 20)
            {
                decompressed_data = huffman_decode(decompressed_data);
            }
            
            if (settings_.use_run_length_encoding && decompressed_data.size() > 10)
            {
                decompressed_data = run_length_decode(decompressed_data);
            }

            size_t data_offset = 0;
            size_t field_index = 0;

            // Apply entity ID change
            if (delta.changed_fields.test(field_index))
            {
                if (data_offset + sizeof(uint32_t) <= decompressed_data.size())
                {
                    std::memcpy(&result.entity_id, &decompressed_data[data_offset], sizeof(uint32_t));
                    data_offset += sizeof(uint32_t);
                }
            }
            field_index++;

            // Apply position delta
            if (delta.changed_fields.test(field_index))
            {
                glm::vec3 position_delta = extract_compressed_vector3(decompressed_data, data_offset, settings_.position_precision);
                result.position = base_state.position + position_delta;
            }
            field_index++;

            // Apply rotation
            if (delta.changed_fields.test(field_index))
            {
                result.rotation = extract_compressed_quaternion(decompressed_data, data_offset);
            }
            field_index++;

            // Apply velocity
            if (delta.changed_fields.test(field_index))
            {
                result.velocity = extract_compressed_vector3(decompressed_data, data_offset, settings_.velocity_precision);
            }
            field_index++;

            result.sequence_number = delta.current_sequence;
            
            // Update statistics
            stats_.deltas_applied++;
            
            return result;
        }

        auto DeltaCompression::store_baseline(uint32_t sequence, const void* state, size_t size) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            std::vector<uint8_t> data(size);
            std::memcpy(data.data(), state, size);
            baselines_[sequence] = std::move(data);
        }

        auto DeltaCompression::get_baseline(uint32_t sequence) -> std::optional<std::vector<uint8_t>>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = baselines_.find(sequence);
            if (it != baselines_.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        auto DeltaCompression::cleanup_old_baselines(uint32_t oldest_sequence) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = baselines_.begin();
            while (it != baselines_.end())
            {
                if (it->first < oldest_sequence)
                {
                    it = baselines_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        auto DeltaCompression::get_statistics() const -> CompressionStats
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return stats_;
        }

        // Compression helper methods
        auto DeltaCompression::compress_float(float value, float precision) -> uint32_t
        {
            return static_cast<uint32_t>(std::round(value / precision));
        }

        auto DeltaCompression::decompress_float(uint32_t compressed, float precision) -> float
        {
            return static_cast<float>(compressed) * precision;
        }

        auto DeltaCompression::huffman_encode(const std::vector<uint8_t>& data) -> std::vector<uint8_t>
        {
            // Simplified Huffman encoding - in production, use a proper implementation
            // For now, just return the original data with a marker
            std::vector<uint8_t> result;
            result.push_back(0xFF); // Huffman marker
            result.insert(result.end(), data.begin(), data.end());
            return result;
        }

        auto DeltaCompression::huffman_decode(const std::vector<uint8_t>& data) -> std::vector<uint8_t>
        {
            // Check for Huffman marker
            if (!data.empty() && data[0] == 0xFF)
            {
                return std::vector<uint8_t>(data.begin() + 1, data.end());
            }
            return data;
        }

        auto DeltaCompression::run_length_encode(const std::vector<uint8_t>& data) -> std::vector<uint8_t>
        {
            std::vector<uint8_t> result;
            result.push_back(0xFE); // RLE marker
            
            for (size_t i = 0; i < data.size(); )
            {
                uint8_t value = data[i];
                uint8_t count = 1;
                
                // Count consecutive same values (max 255)
                while (i + count < data.size() && count < 255 && data[i + count] == value)
                {
                    count++;
                }
                
                // Store count and value
                result.push_back(count);
                result.push_back(value);
                
                i += count;
            }
            
            // Only use RLE if it actually saves space
            return (result.size() < data.size()) ? result : data;
        }

        auto DeltaCompression::run_length_decode(const std::vector<uint8_t>& data) -> std::vector<uint8_t>
        {
            if (data.empty() || data[0] != 0xFE)
            {
                return data;
            }
            
            std::vector<uint8_t> result;
            for (size_t i = 1; i < data.size(); i += 2)
            {
                if (i + 1 < data.size())
                {
                    uint8_t count = data[i];
                    uint8_t value = data[i + 1];
                    result.insert(result.end(), count, value);
                }
            }
            
            return result;
        }

        // Helper functions for vector and quaternion compression
        bool DeltaCompression::position_equals(const glm::vec3& a, const glm::vec3& b, float precision)
        {
            return glm::length(a - b) < precision;
        }
        
        bool DeltaCompression::rotation_equals(const glm::quat& a, const glm::quat& b, float precision)
        {
            float dot_product = glm::dot(a, b);
            return std::abs(dot_product) > 1.0f - precision;
        }
        
        bool DeltaCompression::vector_equals(const glm::vec3& a, const glm::vec3& b, float precision)
        {
            return glm::length(a - b) < precision;
        }
        
        void DeltaCompression::append_compressed_vector3(std::vector<uint8_t>& data, const glm::vec3& vec, float precision)
        {
            // Quantize each component
            uint32_t x = compress_float(vec.x, precision);
            uint32_t y = compress_float(vec.y, precision);
            uint32_t z = compress_float(vec.z, precision);
            
            // Append to data
            auto x_bytes = reinterpret_cast<const uint8_t*>(&x);
            auto y_bytes = reinterpret_cast<const uint8_t*>(&y);
            auto z_bytes = reinterpret_cast<const uint8_t*>(&z);
            
            data.insert(data.end(), x_bytes, x_bytes + sizeof(uint32_t));
            data.insert(data.end(), y_bytes, y_bytes + sizeof(uint32_t));
            data.insert(data.end(), z_bytes, z_bytes + sizeof(uint32_t));
        }
        
        void DeltaCompression::append_compressed_quaternion(std::vector<uint8_t>& data, const glm::quat& quat)
        {
            // Use smallest-three compression
            int largest = 0;
            float largest_val = std::abs(quat.w);
            
            if (std::abs(quat.x) > largest_val) { largest = 1; largest_val = std::abs(quat.x); }
            if (std::abs(quat.y) > largest_val) { largest = 2; largest_val = std::abs(quat.y); }
            if (std::abs(quat.z) > largest_val) { largest = 3; }
            
            // Store largest index and sign
            uint8_t info = static_cast<uint8_t>(largest);
            data.push_back(info);
            
            // Store the three smallest components
            std::array<float, 3> components;
            if (largest == 0) {
                components[0] = quat.x; components[1] = quat.y; components[2] = quat.z;
            } else if (largest == 1) {
                components[0] = quat.w; components[1] = quat.y; components[2] = quat.z;
            } else if (largest == 2) {
                components[0] = quat.w; components[1] = quat.x; components[2] = quat.z;
            } else {
                components[0] = quat.w; components[1] = quat.x; components[2] = quat.y;
            }
            
            for (size_t i = 0; i < components.size(); ++i) {
                uint32_t compressed = compress_float(components[i], 0.001f);
                auto bytes = reinterpret_cast<const uint8_t*>(&compressed);
                data.insert(data.end(), bytes, bytes + sizeof(uint32_t));
            }
        }
        
        glm::vec3 DeltaCompression::extract_compressed_vector3(const std::vector<uint8_t>& data, size_t& offset, float precision)
        {
            if (offset + 12 > data.size()) return glm::vec3(0);
            
            uint32_t x = *reinterpret_cast<const uint32_t*>(&data[offset]);
            offset += sizeof(uint32_t);
            uint32_t y = *reinterpret_cast<const uint32_t*>(&data[offset]);
            offset += sizeof(uint32_t);
            uint32_t z = *reinterpret_cast<const uint32_t*>(&data[offset]);
            offset += sizeof(uint32_t);
            
            return glm::vec3(
                decompress_float(x, precision),
                decompress_float(y, precision),
                decompress_float(z, precision)
            );
        }
        
        glm::quat DeltaCompression::extract_compressed_quaternion(const std::vector<uint8_t>& data, size_t& offset)
        {
            if (offset + 13 > data.size()) return glm::quat(1, 0, 0, 0);
            
            uint8_t info = data[offset++];
            int largest = info & 0x3;
            
            std::array<float, 3> components;
            for (int i = 0; i < 3; ++i) {
                uint32_t compressed = *reinterpret_cast<const uint32_t*>(&data[offset]);
                offset += sizeof(uint32_t);
                components[i] = decompress_float(compressed, 0.001f);
            }
            
            // Reconstruct quaternion
            float a = components[0], b = components[1], c = components[2];
            float largest_val = std::sqrt(std::max(0.0f, 1.0f - (a*a + b*b + c*c)));
            
            switch (largest) {
                case 0: return glm::quat(largest_val, a, b, c);
                case 1: return glm::quat(a, largest_val, b, c);
                case 2: return glm::quat(a, b, largest_val, c);
                case 3: return glm::quat(a, b, c, largest_val);
                default: return glm::quat(1, 0, 0, 0);
            }
        }
    }
}
