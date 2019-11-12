#include <mirrage/graphic/descriptor_sets.hpp>

#include <mirrage/graphic/device.hpp>

#include <mirrage/utils/log.hpp>

#include <gsl/gsl>


namespace mirrage::graphic {

	DescriptorSet::DescriptorSet(Descriptor_pool*            pool,
	                             Descriptor_pool_chunk_index chunk_index,
	                             vk::DescriptorSet           set,
	                             std::int32_t                reserved_bindings)
	  : _pool(pool), _chunk(chunk_index), _set(set), _reserved_bindings(reserved_bindings)
	{
	}
	DescriptorSet::DescriptorSet(DescriptorSet&& rhs) noexcept
	  : _pool(rhs._pool)
	  , _chunk(rhs._chunk)
	  , _set(std::move(rhs._set))
	  , _reserved_bindings(std::move(rhs._reserved_bindings))
	{
		rhs._pool = nullptr;
	}
	DescriptorSet& DescriptorSet::operator=(DescriptorSet&& rhs) noexcept
	{
		MIRRAGE_INVARIANT(this != &rhs, "move to self");

		if(_pool) {
			_pool->_free_descriptor_set(_set, _chunk, _reserved_bindings);
		}

		_pool              = std::move(rhs._pool);
		_chunk             = std::move(rhs._chunk);
		_set               = std::move(rhs._set);
		_reserved_bindings = std::move(rhs._reserved_bindings);
		rhs._pool          = nullptr;

		return *this;
	}
	DescriptorSet::~DescriptorSet()
	{
		if(_pool) {
			_pool->_free_descriptor_set(_set, _chunk, _reserved_bindings);
		}
	}


	auto Descriptor_pool::create_descriptor(vk::DescriptorSetLayout layout, std::int32_t bindings)
	        -> DescriptorSet
	{
		MIRRAGE_INVARIANT(bindings > 0, "No bindings required, really?!?!");
		MIRRAGE_INVARIANT(bindings < _chunk_size,
		                  "Requested number of bindings is higher than chunk size ("
		                          << bindings << " < " << _chunk_size << "). Reevaluate life choices?");

		auto lock = std::scoped_lock{_mutex};

		auto retries = 2;
		do {
			retries--;

			auto free_iter = std::find_if(begin(_chunks_free_count),
			                              end(_chunks_free_count),
			                              [&](auto count) { return count >= bindings; });
			if(free_iter == _chunks_free_count.end()) {
				_chunks_free_count.emplace_back(_chunk_size);
				_create_descriptor_pool();
				free_iter = _chunks_free_count.end() - 1;
			}

			*free_iter -= bindings;
			auto  pool_idx = std::distance(_chunks_free_count.begin(), free_iter);
			auto& pool     = _chunks.at(gsl::narrow<std::size_t>(pool_idx));

			auto alloc_info   = vk::DescriptorSetAllocateInfo{*pool, 1, &layout};
			auto c_alloc_info = VkDescriptorSetAllocateInfo(alloc_info);
			auto desc_set     = VkDescriptorSet{};
			auto ret          = vkAllocateDescriptorSets(_device, &c_alloc_info, &desc_set);

			if(ret == VK_SUCCESS) {
				return DescriptorSet(
				        this, gsl::narrow<Descriptor_pool_chunk_index>(pool_idx), desc_set, bindings);
			} else {
				LOG(plog::warning) << "Allocated a new descriptorSetPool (shouldn't happen too often!).";
				*free_iter = 0;
			}

		} while(retries > 0);

		MIRRAGE_FAIL("Unable to allocate descriptor set!");
	}

	Descriptor_pool::Descriptor_pool(vk::Device                                device,
	                                 std::int32_t                              chunk_size,
	                                 std::initializer_list<vk::DescriptorType> types)
	  : _device(device), _chunk_size(chunk_size)
	{
		_pool_sizes.reserve(types.size());
		for(auto type : types) {
			_pool_sizes.emplace_back(type, chunk_size);
		}

		_create_descriptor_pool();
	}

	auto Descriptor_pool::_create_descriptor_pool() -> vk::DescriptorPool
	{
		auto pool = _device.createDescriptorPoolUnique(
		        vk::DescriptorPoolCreateInfo{vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		                                     gsl::narrow<std::uint32_t>(_chunk_size),
		                                     gsl::narrow<std::uint32_t>(_pool_sizes.size()),
		                                     _pool_sizes.data()});

		auto pool_ref = *pool;
		_chunks.emplace_back(std::move(pool));
		return pool_ref;
	}

	void Descriptor_pool::_free_descriptor_set(vk::DescriptorSet&          set,
	                                           Descriptor_pool_chunk_index chunk,
	                                           std::int32_t                reserved_bindings)
	{
		auto lock = std::scoped_lock{_mutex};

		auto idx   = gsl::narrow<std::size_t>(chunk);
		auto pool  = *_chunks.at(idx);
		auto c_set = VkDescriptorSet(set);
		vkFreeDescriptorSets(_device, pool, 1, &c_set);
		set = vk::DescriptorSet{};

		auto& free_count = _chunks_free_count.at(idx);
		free_count += reserved_bindings;
		if(free_count > _chunk_size) {
			LOG(plog::error) << "Free-count of descriptor pool chunk is higher than max (memory "
			                    "corruption?)!";
		}
	}


	namespace {
		auto create_layout(graphic::Device&     device,
		                   vk::Sampler          sampler,
		                   std::uint32_t        image_number,
		                   vk::ShaderStageFlags stages)
		{
			auto bindings = std::vector<vk::DescriptorSetLayoutBinding>();
			bindings.reserve(image_number);

			auto samplers = std::vector<vk::Sampler>(image_number, sampler);

			for(auto i = std::uint32_t(0); i < image_number; i++) {
				bindings.emplace_back(
				        i, vk::DescriptorType::eCombinedImageSampler, 1, stages, samplers.data());
			}

			return device.create_descriptor_set_layout(bindings);
		}
	} // namespace

	Image_descriptor_set_layout::Image_descriptor_set_layout(graphic::Device&     device,
	                                                         vk::Sampler          sampler,
	                                                         std::int32_t         image_number,
	                                                         vk::ShaderStageFlags stages)
	  : _device(device)
	  , _sampler(sampler)
	  , _image_number(image_number)
	  , _layout(create_layout(device, sampler, gsl::narrow<std::uint32_t>(image_number), stages))
	{
	}

	void Image_descriptor_set_layout::update_set(vk::DescriptorSet                    set,
	                                             std::initializer_list<vk::ImageView> images)
	{

		MIRRAGE_INVARIANT(images.size() <= gsl::narrow<std::size_t>(_image_number),
		                  "Number of images (" << images.size() << ") doesn't match size of descriptor set ("
		                                       << _image_number << ")");

		auto desc_images = std::vector<vk::DescriptorImageInfo>();
		desc_images.reserve(images.size());
		for(auto& image : images) {
			desc_images.emplace_back(_sampler, image, vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		auto desc_writes = std::vector<vk::WriteDescriptorSet>();
		desc_writes.reserve(images.size());
		for(auto& desc_image : desc_images) {
			desc_writes.emplace_back(set,
			                         gsl::narrow<uint32_t>(desc_writes.size()),
			                         0,
			                         1,
			                         vk::DescriptorType::eCombinedImageSampler,
			                         &desc_image,
			                         nullptr);
		}

		_device.vk_device()->updateDescriptorSets(
		        gsl::narrow<std::uint32_t>(desc_writes.size()), desc_writes.data(), 0, nullptr);
	}

	auto Image_descriptor_set_layout::create_set(Descriptor_pool&                     pool,
	                                             std::initializer_list<vk::ImageView> images) -> DescriptorSet
	{
		auto set = pool.create_descriptor(layout(), _image_number);
		update_set(*set, images);
		return set;
	}

} // namespace mirrage::graphic
