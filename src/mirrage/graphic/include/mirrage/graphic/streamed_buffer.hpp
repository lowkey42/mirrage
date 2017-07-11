#pragma once

#include <mirrage/graphic/device_memory.hpp>
#include <mirrage/graphic/vk_wrapper.hpp>

#include <vulkan/vulkan.hpp>
#include <gsl/gsl>

#include <cstdint>
#include <memory>
#include <vector>


namespace mirrage {
namespace graphic {

	class Device;

	class Streamed_buffer {
		public:
			Streamed_buffer(Device&, std::size_t capacity, vk::BufferUsageFlags usage);

			void update(vk::DeviceSize dest_offset, gsl::span<const char> data);

			template<class T>
			void update_objs(vk::DeviceSize dest_offset, gsl::span<T> obj) {
				static_assert(std::is_standard_layout<T>::value, "");
				update(dest_offset, gsl::span<const char>(reinterpret_cast<const char*>(obj.data()),
				                                          obj.size_bytes()));
			}

			void flush(Command_buffer cb, vk::PipelineStageFlags next_stage,
			           vk::AccessFlags next_access);

			auto buffer()const noexcept {
				return *_read_buffer.get_or_throw("flush() has never been called on this buffer!");
			}
			auto capacity()const noexcept {return _capacity;}


		private:
			struct Buffer_entry {
				Buffer_entry(Backed_buffer buffer, char* data);

				Backed_buffer buffer;
				char* data;
			};

			std::size_t                 _capacity;
			std::vector<Buffer_entry>   _buffers;
			std::size_t                 _current_buffer_idx = 0;
			util::maybe<Backed_buffer&> _read_buffer;
	};

}
}
