#include <mirrage/graphic/transfer_manager.hpp>

#include <mirrage/graphic/device.hpp>


namespace mirrage {
namespace graphic {


	Static_buffer::Static_buffer(Static_buffer&& rhs)noexcept
	    : _buffer(std::move(rhs._buffer)) {
	}
	Static_buffer& Static_buffer::operator=(Static_buffer&& rhs)noexcept {
		_buffer = std::move(rhs._buffer);
		return *this;
	}

	Static_image::Static_image(Static_image&& rhs)noexcept
	    : _image(std::move(rhs._image))
	    , _mip_count(std::move(rhs._mip_count))
	    , _dimensions(std::move(rhs._dimensions)) {
	}

	void Dynamic_buffer::update(const Command_buffer& cb, vk::DeviceSize offset,
	                            gsl::span<const char> data) {
		INVARIANT(_capacity>=gsl::narrow<std::size_t>(data.size()+offset),
		          "Buffer overflow");
		INVARIANT(data.size() % 4 == 0,
		          "buffer size has to be a multiple of 4: "<<data.size());
		INVARIANT(offset % 4 == 0,
		          "buffer offset has to be a multiple of 4: "<<offset);

		auto buffer_barrier_pre = vk::BufferMemoryBarrier {
			_latest_usage_access, vk::AccessFlagBits::eTransferWrite, 0, 0,
			*_buffer, offset, gsl::narrow<std::uint32_t>(data.size())
		};
		cb.pipelineBarrier(_latest_usage, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags{},
		                   {}, {buffer_barrier_pre}, {});

		cb.updateBuffer(*_buffer, offset, data.size(), data.data());

		auto buffer_barrier_post = vk::BufferMemoryBarrier {
			vk::AccessFlagBits::eTransferWrite, _earliest_usage_access, 0, 0,
			*_buffer, offset, gsl::narrow<std::uint32_t>(data.size())
		};
		cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, _earliest_usage, vk::DependencyFlags{},
		                   {}, {buffer_barrier_post}, {});
	}
	void Dynamic_buffer::_pre_update(const Command_buffer& cb) {
		auto buffer_barrier_pre = vk::BufferMemoryBarrier {
			_latest_usage_access, vk::AccessFlagBits::eTransferWrite, 0, 0,
			*_buffer, 0, VK_WHOLE_SIZE
		};
		cb.pipelineBarrier(_latest_usage, vk::PipelineStageFlagBits::eTransfer, 
		                   vk::DependencyFlags{},
		                   {}, {buffer_barrier_pre}, {});
	}

	void Dynamic_buffer::_do_update(const Command_buffer& cb,
	                                vk::DeviceSize offset,
	                                gsl::span<const char> data) {
		INVARIANT(_capacity>=gsl::narrow<std::size_t>(data.size()+offset),
		          "Buffer overflow");
		INVARIANT(data.size() % 4 == 0,
		          "buffer size has to be a multiple of 4: "<<data.size());
		INVARIANT(offset % 4 == 0,
		          "buffer offset has to be a multiple of 4: "<<offset);
		
		cb.updateBuffer(*_buffer, offset, data.size(), data.data());
	}

	void Dynamic_buffer::_post_update(const Command_buffer& cb) {
		auto buffer_barrier_post = vk::BufferMemoryBarrier {
			vk::AccessFlagBits::eTransferWrite, _earliest_usage_access, 0, 0,
			*_buffer, 0, VK_WHOLE_SIZE
		};
		cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, _earliest_usage,
		                   vk::DependencyFlags{},
		                   {}, {buffer_barrier_post}, {});
	}


	Transfer_manager::Transfer_manager(Device& device, Queue_tag transfer_queue,
	                                   std::size_t max_frames)
	    : _device(device), _queue_family(_device.get_queue_family(transfer_queue))
	    , _queue(_device.get_queue(transfer_queue))
	    , _semaphore(device.create_semaphore())
	    , _command_buffer_pool(device.create_command_buffer_pool(transfer_queue, true, true))
	    , _command_buffers(device, +[](vk::UniqueCommandBuffer& cb) {cb.reset({});},
	                       [&]{return std::move(_command_buffer_pool.create_primary()[0]);}, max_frames) {

		if(!_device.is_unified_memory_architecture()) {
			_buffer_transfers.reserve(128);
		}

		_image_transfers.reserve(128);
	}
	Transfer_manager::~Transfer_manager() {
	}

	auto Transfer_manager::upload_image(vk::ImageType, std::uint32_t owner,
	                                    const Image_dimensions& dimensions,
	                                    vk::Format format,
	                                    std::uint32_t mip_levels,
	                                    std::uint32_t size,
	                                    std::function<void(char*)> write_data,
	                                    bool dedicated) -> Static_image {

		auto stored_mip_levels = std::max(1u, mip_levels);
		auto actual_mip_levels = mip_levels;
		if(actual_mip_levels==0)
			actual_mip_levels = std::floor(std::log2(std::min(dimensions.width, dimensions.height))) + 1;

		// create staging buffer containing data
		auto staging_buffer = _device.create_buffer(vk::BufferCreateInfo({}, size,
		                                                                 vk::BufferUsageFlagBits::eTransferSrc),
		                                            true, Memory_lifetime::temporary);

		const vk::Device* dev = _device.vk_device();

		// fill buffer
		auto ptr = static_cast<char*>(dev->mapMemory(staging_buffer.memory().memory(),
		                                             staging_buffer.memory().offset(),
		                                             size));

		write_data(ptr);

		// extract sizes of mip levels
		auto mip_image_sizes = std::vector<std::uint32_t>();
		mip_image_sizes.reserve(stored_mip_levels);
		for(auto i : util::range(stored_mip_levels)) {
			(void) i;

			auto size = *reinterpret_cast<std::uint32_t*>(ptr);
			size += 3 - ((size + 3) % 4); // mipPadding
			ptr += sizeof(std::uint32_t); // imageSize
			ptr += size;                  // data

			mip_image_sizes.emplace_back(size);
		}

		dev->unmapMemory(staging_buffer.memory().memory());

		auto real_dimensions = Image_dimensions{
		                       std::max(1u, dimensions.width),
		                       std::max(1u, dimensions.height),
		                       std::max(1u, dimensions.depth),
		                       std::max(1u, dimensions.layers) };

		auto image_create_info = vk::ImageCreateInfo {
				vk::ImageCreateFlags{},
				vk::ImageType::e2D,
				format,
				vk::Extent3D(real_dimensions.width, real_dimensions.height, real_dimensions.depth),
				actual_mip_levels,
				real_dimensions.layers,
				vk::SampleCountFlagBits::e1,
				vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eSampled
				| vk::ImageUsageFlagBits::eTransferDst
				| vk::ImageUsageFlagBits::eTransferSrc
		};

		auto final_image = _device.create_image(image_create_info, false,
		                                        Memory_lifetime::normal, dedicated);

		_image_transfers.emplace_back(std::move(staging_buffer), *final_image, size, owner,
		                              actual_mip_levels, stored_mip_levels, mip_levels==0,
		                              real_dimensions, std::move(mip_image_sizes));

		return {std::move(final_image), actual_mip_levels, mip_levels==0,
			    real_dimensions};
	}

	auto Transfer_manager::upload_buffer(vk::BufferUsageFlags usage, std::uint32_t owner,
	                                     std::uint32_t size, std::function<void(char*)> write_data,
	                                     bool dedicated) -> Static_buffer {
		
		auto staging_buffer_usage = _device.is_unified_memory_architecture() ? usage|vk::BufferUsageFlagBits::eTransferDst
		                                                                     : vk::BufferUsageFlagBits::eTransferSrc;

		auto staging_buffer_lifetime = _device.is_unified_memory_architecture() ? Memory_lifetime::normal
		                                                                        : Memory_lifetime::temporary;

		auto staging_buffer = _create_staging_buffer(staging_buffer_usage,
		                                             staging_buffer_lifetime,
		                                             size, write_data);

		if(_device.is_unified_memory_architecture()) {
			// no transfer required
			return {std::move(staging_buffer)};
		}


		auto final_buffer = _device.create_buffer(vk::BufferCreateInfo({}, size,
		                                                               usage|vk::BufferUsageFlagBits::eTransferDst),
		                                          false, Memory_lifetime::normal, dedicated);

		_buffer_transfers.emplace_back(std::move(staging_buffer), *final_buffer, size, owner);

		return {std::move(final_buffer)};
	}

	auto Transfer_manager::_create_staging_buffer(vk::BufferUsageFlags usage,
	                                              Memory_lifetime lifetime,
	                                              std::uint32_t size,
	                                              std::function<void(char*)> write_data) -> Backed_buffer {

		auto staging_buffer = _device.create_buffer(vk::BufferCreateInfo({}, size, usage),
		                                            true, lifetime);

		const vk::Device* dev = _device.vk_device();

		// fill buffer
		auto ptr = static_cast<char*>(dev->mapMemory(staging_buffer.memory().memory(),
		                                             staging_buffer.memory().offset(),
		                                             size));

		write_data(ptr);

		dev->unmapMemory(staging_buffer.memory().memory());

		return staging_buffer;
	}


	auto Transfer_manager::create_dynamic_buffer(vk::DeviceSize size,
	                                             vk::BufferUsageFlags usage,
	                                             vk::PipelineStageFlags earliest_usage,
					                             vk::AccessFlags earliest_usage_access,
								                 vk::PipelineStageFlags latest_usage,
					                             vk::AccessFlags latest_usage_access) -> Dynamic_buffer {
		auto create_info = vk::BufferCreateInfo(
			{}, size, usage|vk::BufferUsageFlagBits::eTransferDst
		);
		return {_device.create_buffer(create_info, false), size,
		        earliest_usage, earliest_usage_access,
		        latest_usage, latest_usage_access};
	}

	auto Transfer_manager::next_frame(vk::CommandBuffer main_queue_commands) -> util::maybe<vk::Semaphore> {
		if(_buffer_transfers.empty() && _image_transfers.empty())
			return util::nothing;

		main_queue_commands.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});


		auto& command_buffer = *_command_buffers.current();

		command_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

		for(auto& t : _buffer_transfers) {
			command_buffer.copyBuffer(*t.src, t.dst, {vk::BufferCopy{0,0,t.size}});

			//  queue family release operation
			auto buffer_barrier = vk::BufferMemoryBarrier {
				vk::AccessFlagBits::eTransferWrite, vk::AccessFlags{}, _queue_family, t.owner,
				t.dst, 0, VK_WHOLE_SIZE
			};
			// dstStageMask should be 0 (based on the spec) but validation layer complains
			command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
			                               vk::PipelineStageFlagBits::eAllCommands,
			                               vk::DependencyFlags{}, {}, {buffer_barrier}, {});

			_device.destroy_after_frame(std::move(t.src));

		// executed in graphics queue
			//  queue family aquire operation
			auto aq_buffer_barrier = vk::BufferMemoryBarrier {
				vk::AccessFlags{}, vk::AccessFlags{}, _queue_family, t.owner,
				t.dst, 0, VK_WHOLE_SIZE
			};
			// srcStageMask should be 0 (based on the spec) but validation layer complains
			main_queue_commands.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
			                                    vk::PipelineStageFlagBits::eAllCommands,
			                                    vk::DependencyFlags{}, {}, {aq_buffer_barrier}, {});
		}
		_buffer_transfers.clear();

		for(auto& t : _image_transfers) {
			_transfer_image(command_buffer, t);

			//  queue family release operation
			auto subresource = vk::ImageSubresourceRange {
				vk::ImageAspectFlagBits::eColor, 0, t.mip_count_actual, 0, t.dimensions.layers
			};

			auto barrier = vk::ImageMemoryBarrier {
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eShaderRead
				| vk::AccessFlagBits::eTransferWrite,
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferDstOptimal,
				_queue_family, t.owner, t.dst, subresource
			};
			command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
			                               vk::PipelineStageFlagBits::eAllCommands,
			                               vk::DependencyFlags{}, {}, {}, {barrier});

			_device.destroy_after_frame(std::move(t.src));


		// executed in graphics queue
			{
				// queue family aquire operation
				auto subresource = vk::ImageSubresourceRange {
					vk::ImageAspectFlagBits::eColor, 0, t.mip_count_actual, 0, t.dimensions.layers
				};

				auto barrier = vk::ImageMemoryBarrier {
					vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead
				               | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferWrite,
					vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferDstOptimal,
					_queue_family, t.owner, t.dst, subresource
				};
				// srcStageMask should be 0 (based on the spec) but validation layer complains
				main_queue_commands.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
				                               vk::PipelineStageFlagBits::eAllCommands,
				                               vk::DependencyFlags{}, {}, {}, {barrier});

				if(t.generate_mips) {
					// generate complete mip chain
					generate_mipmaps(main_queue_commands, t.dst, vk::ImageLayout::eTransferDstOptimal,
					                 vk::ImageLayout::eShaderReadOnlyOptimal,
					                 t.dimensions.width, t.dimensions.height, t.mip_count_actual);

				} else {
					image_layout_transition(main_queue_commands, t.dst,
					                        vk::ImageLayout::eTransferDstOptimal,
					                        vk::ImageLayout::eShaderReadOnlyOptimal,
					                        vk::ImageAspectFlagBits::eColor,
					                        0, t.mip_count_actual);
				}

			}
		}
		_image_transfers.clear();

		command_buffer.end();

		main_queue_commands.end();

		auto submit_info = vk::SubmitInfo {
			0, nullptr, nullptr,
			1, &command_buffer,
			1, &*_semaphore
		};
		_queue.submit({submit_info}, _command_buffers.start_new_frame());

		return *_semaphore;
	}


	void Transfer_manager::_transfer_image(vk::CommandBuffer cb, const Transfer_image_req& t) {
		image_layout_transition(cb, t.dst, vk::ImageLayout::eUndefined,
		                        vk::ImageLayout::eTransferDstOptimal,
		                        vk::ImageAspectFlagBits::eColor, 0, t.mip_count_actual);

		auto regions = std::vector<vk::BufferImageCopy>();
		regions.reserve(t.mip_count_loaded);
		auto offset = std::uint32_t(0);

		for(auto i : util::range(t.mip_count_loaded)) {
			auto size = t.mip_image_sizes[i];
			offset += sizeof(std::uint32_t); // imageSize

			INVARIANT(offset+size <= t.size, "Overflow in _transfer_image");

			auto subresource = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, i, 0,
			                                              t.dimensions.layers};
			auto extent = vk::Extent3D{std::max(1u, t.dimensions.width  >> i),
			                           std::max(1u, t.dimensions.height >> i),
			                           std::max(1u, t.dimensions.depth  >> i)};
			regions.emplace_back(offset, 0, 0, subresource, vk::Offset3D{}, extent);
			offset += size;
		}

		cb.copyBufferToImage(*t.src, t.dst, vk::ImageLayout::eTransferDstOptimal, regions);
	}

}
}
