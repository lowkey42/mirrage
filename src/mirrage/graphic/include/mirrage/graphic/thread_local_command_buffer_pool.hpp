#pragma once

#include <mirrage/graphic/vk_wrapper.hpp>

#include <mirrage/utils/small_vector.hpp>

#include <tsl/robin_map.h>
#include <vulkan/vulkan.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>


namespace mirrage::graphic {

	class Device;

	using Command_pool_group = int;

	struct Group_command_buffer {
		vk::CommandBuffer cmd_buffer;
		bool              empty = true;
	};

	class Thread_local_command_buffer_pool {
	  public:
		Thread_local_command_buffer_pool(Device&, Queue_tag);
		~Thread_local_command_buffer_pool();

		/// adds a new group and returns its id
		/// thread-safe
		auto add_group() -> Command_pool_group;

		/// register a thread for calls to get
		/// thread-safe
		void register_thread(std::thread::id id = std::this_thread::get_id());

		/// creates/returns the command buffer for the current thread and given group
		/// thread-safe, for concurrent get-calls if the thread has been added
		/// must not be called cuncurrently with the other methods
		auto get(Command_pool_group group) -> Group_command_buffer
		{
			auto& map      = _per_thread[_per_thread_head];
			auto  tls_iter = map.find(std::this_thread::get_id());
			auto& tls      = tls_iter.value();

			MIRRAGE_INVARIANT(tls_iter != map.end(),
			                  "Called get from unregistered thread: " << std::this_thread::get_id());

			MIRRAGE_INVARIANT(group > 0
			                          && group <= static_cast<Command_pool_group>(tls.command_buffers.size()),
			                  "Invalid group id");

			auto& entry = tls.command_buffers[group - 1];
			MIRRAGE_INVARIANT(entry.cmd_buffer, "Invalid command buffer in cache");

			auto copy   = entry;
			entry.empty = false;
			return copy;
		}

		/// resets all command pools that are older than max-frames-in-flight
		/// thread-safe
		void pre_frame();

		/// iterate over each command buffer in a group
		/// thread-safe
		template <typename F>
		void foreach_in_group(Command_pool_group group, F&& callback)
		{
			MIRRAGE_INVARIANT(group > 0, "Invalid command_pool_group handle: " << group);

			auto _ = std::scoped_lock(_mutex);
			for(auto&& [_, value] : _per_thread[_per_thread_head]) {
				if(static_cast<Command_pool_group>(value.command_buffers.size()) >= group) {
					if(auto& e = value.command_buffers[group - 1]; !e.empty)
						callback(e.cmd_buffer);
				}
			}
		}

		void execute_group(Command_pool_group group, vk::CommandBuffer dst_buffer);

	  private:
		struct Per_thread_storage {
			vk::UniqueCommandPool                        pool;
			util::small_vector<Group_command_buffer, 32> command_buffers;

			Per_thread_storage() = default;
			Per_thread_storage(vk::UniqueCommandPool pool) : pool(std::move(pool)) {}
			Per_thread_storage(Per_thread_storage&& rhs) noexcept
			  : pool(std::move(rhs.pool)), command_buffers(std::move(rhs.command_buffers))
			{
			}

			auto& operator=(Per_thread_storage&& rhs) noexcept
			{
				pool            = std::move(rhs.pool);
				command_buffers = std::move(rhs.command_buffers);
				return *this;
			}

			friend void swap(Per_thread_storage& lhs, Per_thread_storage& rhs) noexcept
			{
				using std::swap;
				swap(lhs.pool, rhs.pool);
				swap(lhs.command_buffers, rhs.command_buffers);
			}
		};

		Device&                         _device;
		Queue_tag                       _queue;
		std::mutex                      _mutex;
		std::atomic<Command_pool_group> _next_group_id{1};

		util::small_vector<tsl::robin_map<std::thread::id, Per_thread_storage>, 4> _per_thread;
		std::size_t                                                                _per_thread_head = 0;
	};

} // namespace mirrage::graphic
