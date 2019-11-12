#include <mirrage/graphic/mesh.hpp>

#include <mirrage/graphic/device.hpp>


namespace mirrage::graphic {

	Mesh::Mesh(Device&                    device,
	           std::uint32_t              owner_qfamily,
	           std::uint32_t              vertex_count,
	           std::uint32_t              index_count,
	           std::function<void(char*)> write_vertices,
	           std::function<void(char*)> write_indices)
	  : _buffer(device.transfer().upload_buffer(
	          vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
	          owner_qfamily,
	          gsl::narrow<std::int32_t>(vertex_count + index_count),
	          [&](char* dest) {
		          write_vertices(dest);
		          write_indices(dest + vertex_count);
	          }))
	  , _index_offset(vertex_count)
	  , _indices(std::uint32_t(index_count / sizeof(std::uint32_t)))
	{
	}

	void Mesh::bind(vk::CommandBuffer cb, std::uint32_t vertex_binding) const
	{
		cb.bindVertexBuffers(vertex_binding, {_buffer.buffer()}, {0});
		cb.bindIndexBuffer(_buffer.buffer(), _index_offset, vk::IndexType::eUint32);
	}
} // namespace mirrage::graphic
