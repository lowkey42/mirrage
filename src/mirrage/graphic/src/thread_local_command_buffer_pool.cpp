#include <mirrage/graphic/thread_local_command_buffer_pool.hpp>

#include <mirrage/graphic/device.hpp>

#include <vulkan/vulkan.hpp>

#include <algorithm>


namespace mirrage::graphic {

	Thread_local_command_buffer_pool::Thread_local_command_buffer_pool(Device& device, Queue_tag queue)
	  : _device(device), _queue(queue)
	{
		_per_thread.resize(device.max_frames_in_flight());
	}
	Thread_local_command_buffer_pool::~Thread_local_command_buffer_pool()
	{
		auto _ = std::scoped_lock(_mutex);
		_device.destroy_after_frame(std::move(_per_thread));
	}

	auto Thread_local_command_buffer_pool::add_group() -> Command_pool_group { return _next_group_id++; }

	void Thread_local_command_buffer_pool::register_thread(std::thread::id id)
	{
		auto _ = std::scoped_lock(_mutex);
		for(auto& map : _per_thread) {
			if(auto iter = map.find(std::this_thread::get_id()); iter == map.end()) {
				map.emplace(std::piecewise_construct,
				            std::forward_as_tuple(std::this_thread::get_id()),
				            std::forward_as_tuple(_device.create_vk_command_pool(_queue, false, true)));
			}
		}
	}

	void Thread_local_command_buffer_pool::pre_frame()
	{
		auto _            = std::scoped_lock(_mutex);
		_per_thread_head  = (_per_thread_head + 1u) % _per_thread.size();
		auto&      tls    = _per_thread[_per_thread_head];
		const auto groups = _next_group_id.load();

		for(auto iter = tls.begin(); iter != tls.end(); iter++) {
			auto& value = iter.value();
			_device.vk_device()->resetCommandPool(*value.pool, {});
			for(auto& e : value.command_buffers) {
				e.empty = true;
			}

			auto curr_groups = static_cast<Command_pool_group>(value.command_buffers.size());
			if(curr_groups < groups) {
				auto new_buffers = _device.vk_device()->allocateCommandBuffers(vk::CommandBufferAllocateInfo(
				        *value.pool, vk::CommandBufferLevel::eSecondary, groups - curr_groups));

				value.command_buffers.reserve(groups);
				for(auto& b : new_buffers) {
					value.command_buffers.emplace_back(Group_command_buffer{std::move(b)});
				}
			}
		}
	}

	void Thread_local_command_buffer_pool::execute_group(Command_pool_group group,
	                                                     vk::CommandBuffer  dst_buffer)
	{
		foreach_in_group(group, [&](auto cmd_buffer) {
			cmd_buffer.end();
			dst_buffer.executeCommands(cmd_buffer);
		});
	}

} // namespace mirrage::graphic
