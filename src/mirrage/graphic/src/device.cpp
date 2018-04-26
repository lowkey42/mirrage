#include <mirrage/graphic/device.hpp>

#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/graphic/texture.hpp>
#include <mirrage/graphic/window.hpp>

#include <mirrage/asset/asset_manager.hpp>

#include <gsl/gsl>


namespace mirrage::graphic {

	struct Device::Asset_loaders {
		asset::Asset_manager& assets;

		Asset_loaders(asset::Asset_manager& assets, Device& device, Queue_tag default_draw_queue)
		  : assets(assets)
		{
			auto draw_queue = device.get_queue_family(default_draw_queue);

			assets.create_stateful_loader<Pipeline_cache>(*device.vk_device());
			assets.create_stateful_loader<Texture<Image_type::single_1d>>(device, draw_queue);
			assets.create_stateful_loader<Texture<Image_type::single_2d>>(device, draw_queue);
			assets.create_stateful_loader<Texture<Image_type::single_3d>>(device, draw_queue);
			assets.create_stateful_loader<Texture<Image_type::array_1d>>(device, draw_queue);
			assets.create_stateful_loader<Texture<Image_type::array_2d>>(device, draw_queue);
			assets.create_stateful_loader<Texture<Image_type::array_3d>>(device, draw_queue);
			assets.create_stateful_loader<Texture<Image_type::cubemap>>(device, draw_queue);
			assets.create_stateful_loader<Texture<Image_type::array_cubemap>>(device, draw_queue);
			assets.create_stateful_loader<Shader_module>(device);
		}
		~Asset_loaders()
		{
			assets.remove_stateful_loader<Pipeline_cache>();
			assets.remove_stateful_loader<Texture<Image_type::single_1d>>();
			assets.remove_stateful_loader<Texture<Image_type::single_2d>>();
			assets.remove_stateful_loader<Texture<Image_type::single_3d>>();
			assets.remove_stateful_loader<Texture<Image_type::array_1d>>();
			assets.remove_stateful_loader<Texture<Image_type::array_2d>>();
			assets.remove_stateful_loader<Texture<Image_type::array_3d>>();
			assets.remove_stateful_loader<Texture<Image_type::cubemap>>();
			assets.remove_stateful_loader<Texture<Image_type::array_cubemap>>();
			assets.remove_stateful_loader<Shader_module>();
		}
	};

	Device::Device(Context&               context,
	               asset::Asset_manager&  assets,
	               vk::UniqueDevice       device,
	               vk::PhysicalDevice     gpu,
	               Queue_tag              transfer_queue,
	               Queue_tag              default_draw_queue,
	               Queue_family_mapping   queue_mapping,
	               Swapchain_create_infos swapchains,
	               bool                   dedicated_alloc_supported)
	  : util::Registered<Device, Context>(context)
	  , _device(std::move(device))
	  , _gpu(gpu)
	  , _assets(assets)
	  , _gpu_properties(gpu.getProperties())
	  , _pipeline_cache(load_main_pipeline_cache(*this, assets))
	  , _queue_family_mappings(std::move(queue_mapping))
	  , _memory_allocator(*_device, _gpu, dedicated_alloc_supported)
	  , _transfer_manager(*this, transfer_queue, 2)
	  , _delete_queue(*this, 2, false)
	  , _depth_format(get_supported_format({vk::Format::eD24UnormS8Uint,
	                                        vk::Format::eD32SfloatS8Uint,
	                                        vk::Format::eD32Sfloat,
	                                        vk::Format::eD16Unorm,
	                                        vk::Format::eD16UnormS8Uint},
	                                       vk::FormatFeatureFlagBits::eDepthStencilAttachment,
	                                       Format_usage::image_optimal)
	                          .get_or_throw("No depth format supported"
	                                        " by device"))
	  , _depth_stencil_format(get_supported_format({vk::Format::eD24UnormS8Uint,
	                                                vk::Format::eD32SfloatS8Uint,
	                                                vk::Format::eD16UnormS8Uint},
	                                               vk::FormatFeatureFlagBits::eDepthStencilAttachment,
	                                               Format_usage::image_optimal)
	                                  .get_or_throw("No depth-stencil format "
	                                                "supported by device"))

	  , _r_format(get_supported_format({vk::Format::eR8Unorm},
	                                   vk::FormatFeatureFlagBits::eBlitDst
	                                           | vk::FormatFeatureFlagBits::eBlitSrc
	                                           | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                   Format_usage::image_optimal))
	  , _rg_format(get_supported_format({vk::Format::eR8G8Unorm},
	                                    vk::FormatFeatureFlagBits::eBlitDst
	                                            | vk::FormatFeatureFlagBits::eBlitSrc
	                                            | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                    Format_usage::image_optimal))
	  , _rgb_format(get_supported_format({vk::Format::eR8G8B8Unorm},
	                                     vk::FormatFeatureFlagBits::eBlitDst
	                                             | vk::FormatFeatureFlagBits::eBlitSrc
	                                             | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                     Format_usage::image_optimal))
	  , _rgba_format(get_supported_format({vk::Format::eR8G8B8A8Unorm},
	                                      vk::FormatFeatureFlagBits::eBlitDst
	                                              | vk::FormatFeatureFlagBits::eBlitSrc
	                                              | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                      Format_usage::image_optimal))

	  , _sr_format(get_supported_format({vk::Format::eR8Srgb},
	                                    vk::FormatFeatureFlagBits::eBlitDst
	                                            | vk::FormatFeatureFlagBits::eBlitSrc
	                                            | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                    Format_usage::image_optimal))
	  , _srg_format(get_supported_format({vk::Format::eR8G8Srgb},
	                                     vk::FormatFeatureFlagBits::eBlitDst
	                                             | vk::FormatFeatureFlagBits::eBlitSrc
	                                             | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                     Format_usage::image_optimal))
	  , _srgb_format(get_supported_format({vk::Format::eR8G8B8Srgb},
	                                      vk::FormatFeatureFlagBits::eBlitDst
	                                              | vk::FormatFeatureFlagBits::eBlitSrc
	                                              | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                      Format_usage::image_optimal))
	  , _srgba_format(get_supported_format({vk::Format::eR8G8B8A8Srgb},
	                                       vk::FormatFeatureFlagBits::eBlitDst
	                                               | vk::FormatFeatureFlagBits::eBlitSrc
	                                               | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                       Format_usage::image_optimal))
	  , _device_specific_asset_loaders(std::make_unique<Asset_loaders>(assets, *this, default_draw_queue))
	{

		for(auto& sc_info : swapchains) {
			_swapchains.emplace(
			        sc_info.first,
			        Swapchain{*_device, _gpu, *std::get<0>(sc_info.second), std::get<1>(sc_info.second)});
		}
	}

	Device::~Device()
	{
		wait_idle();
		backup_caches();

		print_memory_usage(std::cerr);

		_delete_queue.clear();
		_memory_allocator.shrink_to_fit();

		print_memory_usage(std::cerr);
	}
	void Device::print_memory_usage(std::ostream& log) const
	{
		auto mem_usage = _memory_allocator.usage_statistic();

		log << "Memory usage: " << mem_usage.used << "/" << mem_usage.reserved << "\n";
		log << "  Types:\n";
		for(auto& t : mem_usage.types) {
			log << "    " << t.first << " : " << t.second.used << "/" << t.second.reserved << "\n";
			log << "      "
			    << "temporary : " << t.second.temporary_pool.used << "/" << t.second.temporary_pool.reserved
			    << " in " << t.second.temporary_pool.blocks << " blocks\n";

			log << "      "
			    << "normal    : " << t.second.normal_pool.used << "/" << t.second.normal_pool.reserved
			    << " in " << t.second.normal_pool.blocks << " blocks\n";

			log << "      "
			    << "persistent : " << t.second.persistent_pool.used << "/"
			    << t.second.persistent_pool.reserved << " in " << t.second.persistent_pool.blocks
			    << " blocks\n";
		}

		log << std::endl;
	}

	void Device::backup_caches()
	{
		if(_pipeline_cache.ready()) {
			_assets.save(_pipeline_cache);
		}
	}

	auto Device::get_queue(Queue_tag tag) -> vk::Queue
	{
		auto real_familiy = _queue_family_mappings.find(tag);
		MIRRAGE_INVARIANT(real_familiy != _queue_family_mappings.end(),
		                  "Unknown queue family tag: " << tag.str());

		return _device->getQueue(std::get<0>(real_familiy->second), std::get<1>(real_familiy->second));
	}
	auto Device::get_queue_family(Queue_tag tag) -> std::uint32_t
	{
		auto real_familiy = _queue_family_mappings.find(tag);
		MIRRAGE_INVARIANT(real_familiy != _queue_family_mappings.end(),
		                  "Unknown queue family tag: " << tag.str());

		return std::get<0>(real_familiy->second);
	}

	auto Device::create_render_pass_builder() -> Render_pass_builder
	{
		return Render_pass_builder{*_device, **_pipeline_cache, _assets};
	}

	auto Device::create_semaphore() -> vk::UniqueSemaphore { return _device->createSemaphoreUnique({}); }

	auto Device::create_buffer(vk::BufferCreateInfo info,
	                           bool                 host_visible,
	                           Memory_lifetime      lifetime,
	                           bool                 dedicated) -> Backed_buffer
	{
#ifdef VK_NV_dedicated_allocation
		auto dedicated_alloc_info = vk::DedicatedAllocationBufferCreateInfoNV{dedicated};
		if(dedicated) {
			info.pNext = &dedicated_alloc_info;
		}
#endif

		auto buffer = _device->createBufferUnique(info);
		auto mem    = [&] {
            if(dedicated) {
                return _memory_allocator.alloc_dedicated(*buffer, host_visible).get_or_throw();
            }

            auto mem_req = _device->getBufferMemoryRequirements(*buffer);
            return _memory_allocator
                    .alloc(mem_req.size, mem_req.alignment, mem_req.memoryTypeBits, host_visible, lifetime)
                    .get_or_throw();
		}();

		_device->bindBufferMemory(*buffer, mem.memory(), mem.offset());

		return {std::move(buffer), std::move(mem)};
	}

	auto Device::create_image(vk::ImageCreateInfo info,
	                          bool                host_visible,
	                          Memory_lifetime     lifetime,
	                          bool                dedicated) -> Backed_image
	{
#ifdef VK_NV_dedicated_allocation
		auto dedicated_alloc_info = vk::DedicatedAllocationImageCreateInfoNV{dedicated};
		if(dedicated) {
			info.pNext = &dedicated_alloc_info;
		}
#endif

		auto image = _device->createImageUnique(info);
		auto mem   = [&] {
            // called first because bindImageMemory without getImageMemoryRequirements is a warning
            //   in the validation layers
            auto mem_req = _device->getImageMemoryRequirements(*image);

            if(dedicated) {
                return _memory_allocator.alloc_dedicated(*image, host_visible).get_or_throw();
            }

            return _memory_allocator
                    .alloc(mem_req.size, mem_req.alignment, mem_req.memoryTypeBits, host_visible, lifetime)
                    .get_or_throw();
		}();

		_device->bindImageMemory(*image, mem.memory(), mem.offset());

		return {std::move(image), std::move(mem)};
	}

	auto Device::create_image_view(vk::Image            image,
	                               vk::Format           format,
	                               std::uint32_t        base_mipmap,
	                               std::uint32_t        mipmap_levels,
	                               vk::ImageAspectFlags aspect,
	                               vk::ImageViewType    type,
	                               vk::ComponentMapping mapping) -> vk::UniqueImageView
	{
		auto range = vk::ImageSubresourceRange{aspect, base_mipmap, mipmap_levels, 0, 1};
		return _device->createImageViewUnique(
		        vk::ImageViewCreateInfo{vk::ImageViewCreateFlags{}, image, type, format, mapping, range});
	}

	auto Device::create_sampler(std::uint32_t          mip_levels,
	                            vk::SamplerAddressMode address_mode,
	                            vk::BorderColor        border_color,
	                            vk::Filter             filter,
	                            vk::SamplerMipmapMode  mipmap_mode,
	                            bool                   anisotropic,
	                            vk::CompareOp          depth_compare_op) -> vk::UniqueSampler
	{
		auto max_aniso = anisotropic ? 16.f : 0.f;

		return _device->createSamplerUnique(vk::SamplerCreateInfo{
		        vk::SamplerCreateFlags{},
		        filter,
		        filter,
		        mipmap_mode,
		        address_mode,
		        address_mode,
		        address_mode,
		        0.f,
		        anisotropic,
		        max_aniso,
		        depth_compare_op != vk::CompareOp::eAlways,
		        depth_compare_op,
		        0.f,
		        float(mip_levels),
		        border_color,
		});
	}


	auto Device::create_command_buffer_pool(Queue_tag queue_family, bool resetable, bool short_lived)
	        -> Command_buffer_pool
	{
		auto real_familiy = _queue_family_mappings.find(queue_family);
		MIRRAGE_INVARIANT(real_familiy != _queue_family_mappings.end(),
		                  "Unknown queue family tag: " << queue_family.str());

		auto flags = vk::CommandPoolCreateFlags{};
		if(resetable)
			flags = flags | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

		if(short_lived)
			flags = flags | vk::CommandPoolCreateFlagBits::eTransient;

		return {*_device,
		        _device->createCommandPoolUnique(
		                vk::CommandPoolCreateInfo{flags, std::get<0>(real_familiy->second)})};
	}

	auto Device::create_descriptor_set_layout(gsl::span<const vk::DescriptorSetLayoutBinding> bindings)
	        -> vk::UniqueDescriptorSetLayout
	{
		return _device->createDescriptorSetLayoutUnique(
		        vk::DescriptorSetLayoutCreateInfo{vk::DescriptorSetLayoutCreateFlags{},
		                                          gsl::narrow<std::uint32_t>(bindings.size()),
		                                          bindings.data()});
	}
	auto Device::create_descriptor_set_layout(const vk::DescriptorSetLayoutBinding& binding)
	        -> vk::UniqueDescriptorSetLayout
	{
		return _device->createDescriptorSetLayoutUnique(
		        vk::DescriptorSetLayoutCreateInfo{vk::DescriptorSetLayoutCreateFlags{}, 1, &binding});
	}

	auto Device::create_descriptor_pool(std::uint32_t                             chunk_size,
	                                    std::initializer_list<vk::DescriptorType> types) -> Descriptor_pool
	{
		return {*_device, chunk_size, types};
	}

	auto Device::create_fence() -> Fence { return Fence{*_device}; }

	auto Device::get_supported_format(std::initializer_list<vk::Format> formats,
	                                  vk::FormatFeatureFlags            flags,
	                                  Format_usage                      usage) -> util::maybe<vk::Format>
	{

		for(auto format : formats) {
			auto props    = _gpu.getFormatProperties(format);
			auto features = [&] {
				switch(usage) {
					case Format_usage::buffer: return props.bufferFeatures;
					case Format_usage::image_linear: return props.linearTilingFeatures;
					case Format_usage::image_optimal: return props.optimalTilingFeatures;
				}
				MIRRAGE_FAIL("Unreachable");
			}();

			if((features & flags) == flags) {
				return format;
			}
		}

		return util::nothing;
	}
} // namespace mirrage::graphic
