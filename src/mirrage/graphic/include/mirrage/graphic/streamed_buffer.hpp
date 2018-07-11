#pragma once

#include <mirrage/graphic/device_memory.hpp>
#include <mirrage/graphic/vk_wrapper.hpp>

#include <gsl/gsl>
#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <memory>
#include <vector>


namespace mirrage::graphic {

	class Device;

	class Streamed_buffer {
	  public:
		Streamed_buffer(Device&, std::int32_t capacity, vk::BufferUsageFlags usage);

		void update(std::int32_t dest_offset, gsl::span<const char> data);

		template <class T>
		void update_objs(std::int32_t dest_offset, gsl::span<T> obj)
		{
			static_assert(std::is_standard_layout<T>::value, "");
			update(dest_offset,
			       gsl::span<const char>(reinterpret_cast<const char*>(obj.data()), obj.size_bytes()));
		}

		void flush(Command_buffer cb, vk::PipelineStageFlags next_stage, vk::AccessFlags next_access);

		auto buffer() const noexcept
		{
			return *_read_buffer.get_or_throw("flush() has never been called on this buffer!");
		}
		auto capacity() const noexcept { return _capacity; }


	  private:
		struct Buffer_entry {
			Buffer_entry(Backed_buffer buffer, char* data);

			Backed_buffer buffer;
			char*         data;
		};

		std::int32_t                _capacity;
		std::vector<Buffer_entry>   _buffers;
		std::int32_t                _current_buffer_idx = 0;
		util::maybe<Backed_buffer&> _read_buffer;
	};
} // namespace mirrage::graphic
