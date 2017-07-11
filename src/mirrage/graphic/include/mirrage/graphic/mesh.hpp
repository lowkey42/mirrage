#pragma once

#include <mirrage/graphic/device_memory.hpp>
#include <mirrage/graphic/texture.hpp>
#include <mirrage/graphic/transfer_manager.hpp>
#include <mirrage/graphic/vk_wrapper.hpp>

#include <vulkan/vulkan.hpp>

#include <gsl/gsl>

#include <memory>
#include <cstdint>


namespace lux {
namespace graphic {

	template<typename T>
	inline auto to_bytes(gsl::span<T> data) {
		return gsl::span<const char>(reinterpret_cast<const char*>(data.data()), data.size_bytes());
	}
	
	class Mesh {
		public:
			template<class V>
			Mesh(Device& device, std::uint32_t owner_qfamily,
			     gsl::span<const V> vertices, gsl::span<const std::uint32_t> indices)
			    : Mesh(device, owner_qfamily, to_bytes(vertices), indices) {
				static_assert(std::is_standard_layout<V>::value, "");
			}

			Mesh(Device& device, std::uint32_t owner_qfamily,
			     gsl::span<const char> vertices, gsl::span<const std::uint32_t> indices)
				: Mesh(device, owner_qfamily, vertices.size_bytes(), indices.size_bytes(),
				       [&](char* dest) {(void)std::memcmp(dest, vertices.data(), vertices.size_bytes());},
				       [&](char* dest) {(void)std::memcmp(dest, indices.data(), indices.size_bytes());}) {
			}

			Mesh(Device& device, std::uint32_t owner_qfamily,
			     std::uint32_t vertex_count, std::uint32_t index_count,
			     std::function<void(char*)> write_vertices, std::function<void(char*)> write_indices);


			void bind(vk::CommandBuffer, std::uint32_t vertex_binding);

			auto index_count()const noexcept {return _indices;}


		private:
			Static_buffer  _buffer;
			vk::DeviceSize _index_offset;
			std::size_t    _indices = 0;
	};
	
}
}
