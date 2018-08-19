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

		/// resizes the current buffer, if its smaller and returns true if the buffer has been recreated
		auto resize(std::int32_t new_min_capacity) -> bool;

		auto update(std::int32_t dest_offset, gsl::span<const char> data) -> bool;

		template <class T>
		auto update_objs(std::int32_t dest_offset, gsl::span<T> obj) -> bool
		{
			static_assert(std::is_standard_layout<T>::value, "");
			return update(dest_offset,
			              gsl::span<const char>(reinterpret_cast<const char*>(obj.data()), obj.size_bytes()));
		}

		template <class T, class F>
		auto update_objects(std::int32_t dest_offset, F&& f) -> bool
		{
			static_assert(std::is_standard_layout<T>::value, "");

			auto& buffer    = _buffers[gsl::narrow<std::size_t>(_current_buffer_idx)];
			auto  recreated = _recreate_buffer(buffer);

			auto size = (_capacity - dest_offset) / sizeof(T);
			f(gsl::span<T>(reinterpret_cast<T*>(buffer.data + dest_offset), size));
			return recreated;
		}

		void flush(Command_buffer cb, vk::PipelineStageFlags next_stage, vk::AccessFlags next_access);

		auto write_buffer_index() const noexcept { return _current_buffer_idx; }
		auto read_buffer_index() const noexcept
		{
			return _read_buffer_idx.get_or_throw("flush() has never been called on this buffer!");
		}

		auto write_buffer() const noexcept { return buffer(std::size_t(write_buffer_index())); }
		auto read_buffer() const noexcept
		{
			return *_read_buffer.get_or_throw("flush() has never been called on this buffer!");
		}

		auto buffer_count() const noexcept { return _buffers.size(); }
		auto buffer(std::size_t i) const noexcept -> vk::Buffer { return *_buffers.at(i).buffer; }
		auto capacity() const noexcept { return _capacity; }


	  private:
		struct Buffer_entry {
			Backed_buffer buffer;
			char*         data     = nullptr;
			std::int32_t  capacity = 0;
		};

		Device&                     _device;
		vk::BufferUsageFlags        _usage;
		std::int32_t                _capacity;
		std::vector<Buffer_entry>   _buffers;
		std::int32_t                _current_buffer_idx = 0;
		util::maybe<std::int32_t>   _read_buffer_idx;
		util::maybe<Backed_buffer&> _read_buffer;

		auto _recreate_buffer(Buffer_entry&) -> bool;
	};
} // namespace mirrage::graphic
