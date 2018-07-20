#include <mirrage/graphic/streamed_buffer.hpp>

#include <mirrage/graphic/device.hpp>

#include <gsl/gsl>


namespace mirrage::graphic {

	Streamed_buffer::Streamed_buffer(Device& device, std::int32_t capacity, vk::BufferUsageFlags usage)
	  : _device(device)
	  , _usage(usage)
	  , _capacity(capacity)
	  , _buffers(util::build_vector<Buffer_entry>(device.max_frames_in_flight(), [&](auto, auto& vec) {
		  _recreate_buffer(vec.emplace_back());
	  }))
	{
	}

	auto Streamed_buffer::_recreate_buffer(Buffer_entry& buffer) -> bool
	{
		if(buffer.capacity >= _capacity)
			return false;

		buffer.buffer = {}; // clear first to free up memory

		auto create_info = vk::BufferCreateInfo({}, gsl::narrow<std::uint32_t>(_capacity), _usage);
		buffer.buffer    = _device.create_buffer(create_info, true, Memory_lifetime::normal, true);
		buffer.capacity  = _capacity;
		buffer.data      = buffer.buffer.memory().mapped_addr().get_or_throw(
                "Streamed_buffer GPU memory is not mapped!");
		return true;
	}

	auto Streamed_buffer::resize(std::int32_t new_min_capacity) -> bool
	{
		_capacity = new_min_capacity;

		return _recreate_buffer(gsl::at(_buffers, _current_buffer_idx));
	}

	auto Streamed_buffer::update(std::int32_t dest_offset, gsl::span<const char> data) -> bool
	{
		MIRRAGE_INVARIANT(data.size_bytes() + dest_offset <= _capacity, "Buffer overflow!");

		auto& buffer    = _buffers[gsl::narrow<std::size_t>(_current_buffer_idx)];
		auto  recreated = _recreate_buffer(buffer);

		auto dest = buffer.data + dest_offset;
		std::memcpy(dest, data.data(), gsl::narrow<std::size_t>(data.size_bytes()));

		return recreated;
	}

	void Streamed_buffer::flush(Command_buffer         cb,
	                            vk::PipelineStageFlags next_stage,
	                            vk::AccessFlags        next_access)
	{
		_read_buffer        = _buffers[gsl::narrow<std::size_t>(_current_buffer_idx)].buffer;
		_read_buffer_idx    = _current_buffer_idx;
		_current_buffer_idx = (_current_buffer_idx + 1) % gsl::narrow<std::int32_t>(_buffers.size());

		auto buffer_barrier = vk::BufferMemoryBarrier{vk::AccessFlagBits::eHostWrite,
		                                              next_access,
		                                              0,
		                                              0,
		                                              *_read_buffer.get_or_throw(),
		                                              0,
		                                              VK_WHOLE_SIZE};
		cb.pipelineBarrier(
		        vk::PipelineStageFlagBits::eHost, next_stage, vk::DependencyFlags{}, {}, {buffer_barrier}, {});
	}

} // namespace mirrage::graphic
