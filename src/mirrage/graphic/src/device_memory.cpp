#include <mirrage/graphic/device_memory.hpp>

#include <mirrage/utils/template_utils.hpp>

#include <gsl/gsl>

#include <bitset>
#include <cmath>
#include <mutex>


namespace mirrage::graphic {

	namespace {
		template <typename T>
		constexpr T log2(T n)
		{
			return (n < 2) ? 0 : 1 + log2(n / 2);
		}

		template <std::uint32_t MinSize, std::uint32_t MaxSize>
		class Buddy_block_alloc {
		  public:
			Buddy_block_alloc(const vk::Device&, std::uint32_t type, bool mapable, std::mutex& free_mutex);
			Buddy_block_alloc(const Buddy_block_alloc&) = delete;
			Buddy_block_alloc(Buddy_block_alloc&&)      = delete;
			Buddy_block_alloc& operator=(const Buddy_block_alloc&) = delete;
			Buddy_block_alloc& operator=(Buddy_block_alloc&&) = delete;
			~Buddy_block_alloc()
			{
				if(_allocation_count > 0) {
					LOG(plog::error) << "Still " << _allocation_count
					                 << " unfreed GPU memory allocations left.";
				}
			}

			auto alloc(std::uint32_t size, std::uint32_t alignment) -> util::maybe<Device_memory>;

			void free(std::uint32_t index, std::uint32_t layer);

			auto free_memory() const -> std::uint32_t
			{
				auto free_memory = std::size_t(0);
				for(auto i = std::size_t(0); i < layers; i++) {
					auto free   = _free_blocks[i].size();
					free_memory = total_size / (1L << (i + 1)) * free;
				}

				return gsl::narrow<std::uint32_t>(free_memory);
			}

			auto empty() const noexcept { return !_free_blocks[0].empty(); }

		  private:
			using Free_list = std::vector<std::uint32_t>;

			static constexpr auto layers     = log2(MaxSize) - log2(MinSize) + 1;
			static constexpr auto total_size = (1 << layers) * MinSize;
			static constexpr auto blocks     = (1 << layers) - 1;

			vk::UniqueDeviceMemory _memory;
			char*                  _mapped_addr;

			std::array<Free_list, layers> _free_blocks;
			std::bitset<blocks / 2>       _free_buddies;
			std::int64_t                  _allocation_count = 0;
			std::mutex&                   _free_mutex;

			auto _index_to_offset(std::uint32_t layer, std::uint32_t idx) -> vk::DeviceSize;

			/// split given block, return index of one of the new blocks
			auto _split(std::uint32_t layer, std::uint32_t idx) -> std::uint32_t;

			/// tries to merge the given block with its buddy
			auto _merge(std::uint32_t layer, std::uint32_t idx) -> bool;

			auto _buddies_different(std::uint32_t layer, std::uint32_t idx)
			{
				auto abs_idx = (1L << layer) - 1 + idx;

				MIRRAGE_INVARIANT(abs_idx > 0, "_buddies_different() called for layer 0!");
				return _free_buddies[gsl::narrow<std::size_t>((abs_idx - 1) / 2)];
			}
		};
	} // namespace

	template <std::uint32_t MinSize, std::uint32_t MaxSize>
	class Device_memory_pool {
	  public:
		Device_memory_pool(const vk::Device&, std::uint32_t type, bool mapable);
		~Device_memory_pool() = default;

		auto alloc(std::uint32_t size, std::uint32_t alignment) -> util::maybe<Device_memory>;

		auto shrink_to_fit() -> std::size_t
		{
			auto lock = std::scoped_lock{_mutex};

			auto new_end = std::remove_if(
			        _blocks.begin(), _blocks.end(), [](auto& block) { return block->empty(); });
			auto deleted = std::distance(new_end, _blocks.end());
			if(deleted > 0) {
				LOG(plog::debug) << "Freed " << deleted << "/" << _blocks.size()
				                 << " blocks in allocator for type " << _type;
			}

			_blocks.erase(new_end, _blocks.end());

			return gsl::narrow<std::size_t>(deleted);
		}

		auto usage_statistic() const -> Device_memory_pool_usage
		{
			auto lock = std::scoped_lock{_mutex};

			auto usage = Device_memory_pool_usage{0, 0, _blocks.size()};
			for(auto& block : _blocks) {
				usage.reserved += MaxSize;
				usage.used += MaxSize - block->free_memory();
			}

			return usage;
		}

	  private:
		const vk::Device&  _device;
		std::uint32_t      _type;
		bool               _mapable;
		mutable std::mutex _mutex;

		std::vector<std::unique_ptr<Buddy_block_alloc<MinSize, MaxSize>>> _blocks;
	};


	class Device_heap {
	  public:
		Device_heap(const vk::Device& device, std::uint32_t type, bool mapable)
		  : _device(device)
		  , _type(type)
		  , _mapable(mapable)
		  , _temporary_pool(device, type, mapable)
		  , _normal_pool(device, type, mapable)
		  , _persistent_pool(device, type, mapable)
		{
		}
		Device_heap(const Device_heap&) = delete;
		Device_heap(Device_heap&&)      = delete;
		Device_heap& operator=(const Device_heap&) = delete;
		Device_heap& operator=(Device_heap&&) = delete;

		auto alloc(std::uint32_t size, std::uint32_t alignment, Memory_lifetime lifetime)
		        -> util::maybe<Device_memory>
		{

			auto memory = [&] {
				switch(lifetime) {
					case Memory_lifetime::temporary: return _temporary_pool.alloc(size, alignment);
					case Memory_lifetime::normal: return _normal_pool.alloc(size, alignment);
					case Memory_lifetime::persistent: return _persistent_pool.alloc(size, alignment);
				}
				MIRRAGE_FAIL("Unreachable");
			}();

			if(memory.is_some())
				return memory;

			// single allocation
			auto alloc_info = vk::MemoryAllocateInfo{size, _type};
			auto m          = _device.allocateMemoryUnique(alloc_info);
			auto mem_addr = _mapable ? static_cast<char*>(_device.mapMemory(*m, 0, VK_WHOLE_SIZE)) : nullptr;
			return Device_memory{this,
			                     +[](void* self, std::uint32_t, std::uint32_t, vk::DeviceMemory memory) {
				                     static_cast<Device_heap*>(self)->_device.freeMemory(memory);
			                     },
			                     0,
			                     size,
			                     m.release(),
			                     0,
			                     mem_addr};
		}

		auto shrink_to_fit() -> std::size_t
		{
			return _temporary_pool.shrink_to_fit() + _normal_pool.shrink_to_fit()
			       + _persistent_pool.shrink_to_fit();
		}

		auto usage_statistic() const -> Device_memory_type_usage
		{
			auto temporary_usage  = _temporary_pool.usage_statistic();
			auto normal_usage     = _normal_pool.usage_statistic();
			auto persistent_usage = _persistent_pool.usage_statistic();

			return {temporary_usage.reserved + normal_usage.reserved + persistent_usage.reserved,
			        temporary_usage.used + normal_usage.used + persistent_usage.used,
			        temporary_usage,
			        normal_usage,
			        persistent_usage};
		}

	  private:
		friend class Device_memory_allocator;
		friend class Device_memory;

		const vk::Device& _device;
		std::uint32_t     _type;
		bool              _mapable;

		Device_memory_pool<1024L * 1024 * 4, 256L * 1024 * 1024> _temporary_pool;
		Device_memory_pool<1024L * 1024 * 1, 256L * 1024 * 1024> _normal_pool;
		Device_memory_pool<1024L * 1024 * 1, 256L * 1024 * 1024> _persistent_pool;
	};



	Device_memory::Device_memory(Device_memory&& rhs) noexcept
	  : _owner(rhs._owner)
	  , _deleter(rhs._deleter)
	  , _index(rhs._index)
	  , _layer(rhs._layer)
	  , _memory(std::move(rhs._memory))
	  , _offset(rhs._offset)
	  , _mapped_addr(rhs._mapped_addr)
	{

		rhs._owner = nullptr;
	}
	Device_memory& Device_memory::operator=(Device_memory&& rhs) noexcept
	{
		if(&rhs == this)
			return *this;

		if(_owner) {
			_deleter(_owner, _index, _layer, _memory);
		}

		_owner       = rhs._owner;
		_deleter     = rhs._deleter;
		_index       = rhs._index;
		_layer       = rhs._layer;
		_memory      = rhs._memory;
		_offset      = rhs._offset;
		_mapped_addr = rhs._mapped_addr;

		rhs._owner = nullptr;

		return *this;
	}
	Device_memory::~Device_memory()
	{
		if(_owner) {
			_deleter(_owner, _index, _layer, _memory);
		}
	}

	Device_memory::Device_memory(void*            owner,
	                             Deleter*         deleter,
	                             std::uint32_t    index,
	                             std::uint32_t    layer,
	                             vk::DeviceMemory m,
	                             vk::DeviceSize   o,
	                             char*            mapped_addr)
	  : _owner(owner)
	  , _deleter(deleter)
	  , _index(index)
	  , _layer(layer)
	  , _memory(m)
	  , _offset(o)
	  , _mapped_addr(mapped_addr)
	{
	}


	Device_memory_allocator::Device_memory_allocator(const vk::Device&  device,
	                                                 vk::PhysicalDevice gpu,
	                                                 bool               dedicated_alloc_supported)
	  : _device(device)
	  , _is_unified_memory_architecture(true)
	  , _is_dedicated_allocations_supported(dedicated_alloc_supported)
	{

		auto memory_properties = gpu.getMemoryProperties();

		const auto host_visible_flags =
		        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
		const auto device_local_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;


		_pools.reserve(memory_properties.memoryTypeCount);
		for(auto id : util::range(memory_properties.memoryTypeCount)) {
			auto host_visible = (memory_properties.memoryTypes[id].propertyFlags & host_visible_flags)
			                    == host_visible_flags;

			_pools.emplace_back(std::make_unique<Device_heap>(_device, id, host_visible));
		}

		for(auto i : util::range(memory_properties.memoryTypeCount)) {
			auto host_visible = (memory_properties.memoryTypes[i].propertyFlags & host_visible_flags)
			                    == host_visible_flags;

			auto device_local = (memory_properties.memoryTypes[i].propertyFlags & device_local_flags)
			                    == device_local_flags;

			if(host_visible) {
				_host_visible_pools.emplace_back(i);
			}
			if(device_local) {
				_device_local_pools.emplace_back(i);
			}

			if(host_visible || device_local) {
				_is_unified_memory_architecture &= host_visible && device_local;
			}
		}

		if(_is_unified_memory_architecture) {
			LOG(plog::info) << "Detected a unified memory architecture.";
		}
		if(_is_dedicated_allocations_supported) {
			LOG(plog::info) << "VK_NV_dedicated_allocation enabled";
		}
	}
	Device_memory_allocator::~Device_memory_allocator() = default;

	auto Device_memory_allocator::alloc(std::uint32_t   size,
	                                    std::uint32_t   alignment,
	                                    std::uint32_t   type_mask,
	                                    bool            host_visible,
	                                    Memory_lifetime lifetime) -> util::maybe<Device_memory>
	{

		const auto& pool_ids = host_visible ? _host_visible_pools : _device_local_pools;

		for(auto id : pool_ids) {
			if(type_mask & (1u << id)) {
				auto& pool = _pools[id];

				try {
					return pool->alloc(size, alignment, lifetime);
				} catch(std::system_error& e) {
					auto usage = usage_statistic();
					LOG(plog::error) << "Couldn't allocate block of size " << size
					                 << "\n Usage: " << usage.used << "/" << usage.reserved;
					// free memory and retry
					if(e.code() == vk::Result::eErrorOutOfDeviceMemory && shrink_to_fit() > 0) {
						return pool->alloc(size, alignment, lifetime);
					}
				}
			}
		}

		MIRRAGE_FAIL("No pool on the device matches the requirements. Type mask="
		             << type_mask << ", host visible=" << host_visible);
	}

	auto Device_memory_allocator::alloc_dedicated(vk::Image image, bool host_visible)
	        -> util::maybe<Device_memory>
	{
		return alloc_dedicated({}, image, host_visible);
	}

	auto Device_memory_allocator::alloc_dedicated(vk::Buffer buffer, bool host_visible)
	        -> util::maybe<Device_memory>
	{
		return alloc_dedicated(buffer, {}, host_visible);
	}

	auto Device_memory_allocator::alloc_dedicated(vk::Buffer buffer, vk::Image image, bool host_visible)
	        -> util::maybe<Device_memory>
	{

		const auto& pool_ids = host_visible ? _host_visible_pools : _device_local_pools;

		auto requirements = buffer ? _device.getBufferMemoryRequirements(buffer)
		                           : _device.getImageMemoryRequirements(image);

		for(auto id : pool_ids) {
			if(requirements.memoryTypeBits & (1u << id)) {
				auto alloc_info = vk::MemoryAllocateInfo{requirements.size, id};
#ifdef VK_NV_dedicated_allocation
				auto dedicated_info = vk::DedicatedAllocationMemoryAllocateInfoNV{};

				if(_is_dedicated_allocations_supported) {
					dedicated_info.buffer = buffer;
					dedicated_info.image  = image;
					alloc_info.pNext      = &dedicated_info;
				}
#endif
				LOG(plog::debug) << "Alloc: " << (float(requirements.size) / 1024.f / 1024.f) << " MB";
				auto mem      = _device.allocateMemoryUnique(alloc_info);
				auto mem_addr = host_visible ? static_cast<char*>(_device.mapMemory(*mem, 0, VK_WHOLE_SIZE))
				                             : nullptr;
				return Device_memory{
				        this,
				        +[](void* self, std::uint32_t, std::uint32_t, vk::DeviceMemory memory) {
					        static_cast<Device_memory_allocator*>(self)->_device.freeMemory(memory);
				        },
				        0,
				        0,
				        mem.release(),
				        0,
				        mem_addr};
			}
		}

		MIRRAGE_FAIL("No pool on the device matches the requirements. Type mask="
		             << requirements.memoryTypeBits << ", host visible=" << host_visible);
	}

	auto Device_memory_allocator::shrink_to_fit() -> std::size_t
	{
		auto sum = std::size_t(0);

		for(auto& heap : _pools) {
			if(heap) {
				sum += heap->shrink_to_fit();
			}
		}

		return sum;
	}
	auto Device_memory_allocator::usage_statistic() const -> Device_memory_usage
	{
		auto types    = std::unordered_map<std::uint32_t, Device_memory_type_usage>();
		auto reserved = std::size_t(0);
		auto used     = std::size_t(0);

		auto type = std::uint32_t(0);
		for(auto& heap : _pools) {
			if(heap) {
				auto& t_usage = types[0];
				t_usage       = heap->usage_statistic();
				reserved += t_usage.reserved;
				used += t_usage.used;
			}
			type++;
		}

		return {reserved, used, types};
	}


	// POOL
	template <std::uint32_t MinSize, std::uint32_t MaxSize>
	Device_memory_pool<MinSize, MaxSize>::Device_memory_pool(const vk::Device& device,
	                                                         std::uint32_t     type,
	                                                         bool              mapable)
	  : _device(device), _type(type), _mapable(mapable)
	{
	}

	template <std::uint32_t MinSize, std::uint32_t MaxSize>
	auto Device_memory_pool<MinSize, MaxSize>::alloc(std::uint32_t size, std::uint32_t alignment)
	        -> util::maybe<Device_memory>
	{

		if(size > MaxSize)
			return util::nothing;

		auto lock = std::scoped_lock{_mutex};

		for(auto& block : _blocks) {
			auto m = block->alloc(size, alignment);
			if(m.is_some())
				return m;
		}

		_blocks.emplace_back(
		        std::make_unique<Buddy_block_alloc<MinSize, MaxSize>>(_device, _type, _mapable, _mutex));

		auto m = _blocks.back()->alloc(size, alignment);
		MIRRAGE_INVARIANT(m.is_some(),
		                  "Couldn't allocate " << size << " byte with allignment" << alignment
		                                       << " from newly created block of size " << MaxSize);

		return m;
	}

	namespace {

		template <std::uint32_t MinSize, std::uint32_t MaxSize>
		Buddy_block_alloc<MinSize, MaxSize>::Buddy_block_alloc(const vk::Device& device,
		                                                       std::uint32_t     type,
		                                                       bool              mapable,
		                                                       std::mutex&       free_mutex)
		  : _memory(device.allocateMemoryUnique({MaxSize, type}))
		  , _mapped_addr(mapable ? static_cast<char*>(device.mapMemory(*_memory, 0, VK_WHOLE_SIZE)) : nullptr)
		  , _free_mutex(free_mutex)
		{
			LOG(plog::debug) << "Alloc: " << (MaxSize / 1024.f / 1024.f) << " MB";
			_free_blocks[0].emplace_back(0);
		}

		template <std::uint32_t MinSize, std::uint32_t MaxSize>
		auto Buddy_block_alloc<MinSize, MaxSize>::alloc(std::uint32_t size, std::uint32_t alignment)
		        -> util::maybe<Device_memory>
		{

			if(size < alignment) {
				size = alignment;
			} else if(size % alignment != 0) {
				size += alignment - size % alignment;
			}

			if(size < MinSize) {
				size = MinSize;
			}

			auto layer = gsl::narrow<std::uint32_t>(std::ceil(std::log2(static_cast<float>(size)))
			                                        - log2(MinSize));

			layer = layer < layers ? layers - layer - 1 : 0;

			// find first layer with free block >= size
			auto current_layer = layer;
			while(_free_blocks[current_layer].empty()) {
				if(current_layer == 0) {
					return util::nothing; //< not enough free space
				}

				current_layer--;
			}

			// reserve found block
			auto index = _free_blocks[current_layer].back();
			_free_blocks[current_layer].pop_back();

			if(current_layer > 0) {
				_buddies_different(current_layer, index).flip();
			}

			// unwinde and split all blocks an the way down
			for(; current_layer <= layer; current_layer++) {
				if(current_layer != layer)
					index = _split(current_layer, index);
			}

			auto offset = _index_to_offset(layer, index);

			MIRRAGE_INVARIANT(offset % alignment == 0,
			                  "Resulting offset is not aligned correctly. Expected:"
			                          << alignment << " Offset: " << offset);

			_allocation_count++;

			return Device_memory{this,
			                     +[](void* self, std::uint32_t i, std::uint32_t l, vk::DeviceMemory) {
				                     static_cast<Buddy_block_alloc*>(self)->free(l, i);
			                     },
			                     index,
			                     layer,
			                     *_memory,
			                     offset,
			                     _mapped_addr ? _mapped_addr + offset : nullptr};
		}

		template <std::uint32_t MinSize, std::uint32_t MaxSize>
		void Buddy_block_alloc<MinSize, MaxSize>::free(std::uint32_t layer, std::uint32_t index)
		{
			auto lock = std::scoped_lock{_free_mutex};

			_allocation_count--;

			_free_blocks[layer].emplace_back(index);

			if(layer == 0) {
				MIRRAGE_INVARIANT(index == 0, "index overflow in layer 0, index=" << index);
			} else {
				_buddies_different(layer, index).flip();
				_merge(layer, index);
			}
		}

		template <std::uint32_t MinSize, std::uint32_t MaxSize>
		auto Buddy_block_alloc<MinSize, MaxSize>::_index_to_offset(std::uint32_t layer, std::uint32_t idx)
		        -> vk::DeviceSize
		{
			return total_size / (1L << (layer + 1)) * idx;
		}

		template <std::uint32_t MinSize, std::uint32_t MaxSize>
		auto Buddy_block_alloc<MinSize, MaxSize>::_split(std::uint32_t layer, std::uint32_t idx)
		        -> std::uint32_t
		{
			_buddies_different(layer + 1, idx * 2 + 1) = true;
			_free_blocks[layer + 1].emplace_back(idx * 2 + 1);

			return idx * 2;
		}

		template <std::uint32_t MinSize, std::uint32_t MaxSize>
		auto Buddy_block_alloc<MinSize, MaxSize>::_merge(std::uint32_t layer, std::uint32_t index) -> bool
		{
			if(layer == 0)
				return false;

			auto diff = _buddies_different(layer, index);
			if(diff == false) { // both free
				// remove nodes from freelist and add parent
				auto parent_index = index / 2;
				util::erase_fast(_free_blocks[layer], parent_index * 2);
				util::erase_fast(_free_blocks[layer], parent_index * 2 + 1);

				_free_blocks[layer - 1].emplace_back(parent_index);
				if(layer > 1) {
					_buddies_different(layer - 1, parent_index).flip();
					_merge(layer - 1, parent_index);
				}

				return true;
			}

			return false;
		}
	} // namespace
} // namespace mirrage::graphic
