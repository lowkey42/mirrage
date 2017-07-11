#include <mirrage/graphic/streamed_buffer.hpp>

#include <mirrage/graphic/device.hpp>


namespace mirrage {
namespace graphic {

	Streamed_buffer::Streamed_buffer(Device& device, std::size_t capacity, vk::BufferUsageFlags usage)
	    : _capacity(capacity) {

		auto create_info = vk::BufferCreateInfo(
			{}, capacity, usage
		);

		_buffers.reserve(device.max_frames_in_flight());
		for(auto i : util::range(device.max_frames_in_flight())) {
			(void) i;

			auto buffer        = device.create_buffer(create_info, true, Memory_lifetime::normal, true);
			auto mapped_memory = static_cast<char*>(device.vk_device()->mapMemory(buffer.memory().memory(), 0, capacity))
			                   + buffer.memory().offset();
			_buffers.emplace_back(std::move(buffer), mapped_memory);
		}
	}

	void Streamed_buffer::update(vk::DeviceSize dest_offset,
	                             gsl::span<const char> data) {
		INVARIANT(data.size_bytes()+dest_offset <= _capacity, "Buffer overflow!");

		auto& buffer = _buffers[_current_buffer_idx];

		auto dest = buffer.data + dest_offset;
		std::memcpy(dest, data.data(), data.size_bytes());
	}

	void Streamed_buffer::flush(Command_buffer cb,
	                            vk::PipelineStageFlags next_stage,
	                            vk::AccessFlags next_access) {
		_read_buffer = _buffers[_current_buffer_idx].buffer;
		_current_buffer_idx = (_current_buffer_idx+1) % _buffers.size();

		auto buffer_barrier = vk::BufferMemoryBarrier {
			vk::AccessFlagBits::eHostWrite, next_access, 0, 0,
			*_read_buffer.get_or_throw(), 0, VK_WHOLE_SIZE
		};
		cb.pipelineBarrier(vk::PipelineStageFlagBits::eHost, next_stage,
		                   vk::DependencyFlags{},
		                   {}, {buffer_barrier}, {});
	}

	Streamed_buffer::Buffer_entry::Buffer_entry(Backed_buffer buffer, char* data)
	    : buffer(std::move(buffer)), data(data) {
	}

}
}
