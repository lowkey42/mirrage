#pragma once

#include <mirrage/graphic/device_memory.hpp>
#include <mirrage/graphic/texture.hpp>
#include <mirrage/graphic/transfer_manager.hpp>
#include <mirrage/graphic/vk_wrapper.hpp>

#include <vulkan/vulkan.hpp>

#include <gsl/gsl>

#include <cstdint>
#include <memory>


namespace mirrage::graphic {

	template <typename T>
	inline auto to_bytes(gsl::span<T> data)
	{
		return gsl::span<const char>(reinterpret_cast<const char*>(data.data()), data.size_bytes());
	}

	class Mesh {
	  public:
		template <class VS, class IS>
		Mesh(Device& device, std::uint32_t owner_qfamily, const VS& vertices, const IS& indices)
		  : Mesh(device,
		         owner_qfamily,
		         gsl::narrow<std::uint32_t>(vertices.size() * sizeof(vertices[0])),
		         gsl::narrow<std::uint32_t>(indices.size() * sizeof(indices[0])),
		         [&](char* dest) {
			         std::memcpy(dest, vertices.data(), vertices.size() * sizeof(vertices[0]));
		         },
		         [&](char* dest) { std::memcpy(dest, indices.data(), indices.size() * sizeof(indices[0])); })
		{
		}

		Mesh(Device&                    device,
		     std::uint32_t              owner_qfamily,
		     std::uint32_t              vertex_count,
		     std::uint32_t              index_count,
		     std::function<void(char*)> write_vertices,
		     std::function<void(char*)> write_indices);


		void bind(vk::CommandBuffer, std::uint32_t vertex_binding) const;

		auto index_count() const noexcept { return _indices; }

		auto internal_buffer() const noexcept -> auto& { return _buffer; }
		auto ready() const { return _buffer.transfer_task().ready(); }

	  private:
		Static_buffer  _buffer;
		vk::DeviceSize _index_offset;
		std::uint32_t  _indices = 0;
	};
} // namespace mirrage::graphic
