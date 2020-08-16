#include <mirrage/graphic/transfer_manager.hpp>

#include <mirrage/graphic/device.hpp>

#include <mirrage/utils/ranges.hpp>


namespace mirrage::graphic {

	Static_buffer::Static_buffer(Static_buffer&& rhs) noexcept
	  : _buffer(std::move(rhs._buffer)), _transfer_task(std::move(rhs._transfer_task))
	{
	}
	Static_buffer& Static_buffer::operator=(Static_buffer&& rhs) noexcept
	{
		_buffer        = std::move(rhs._buffer);
		_transfer_task = std::move(rhs._transfer_task);
		return *this;
	}

	Static_image::Static_image(Static_image&& rhs) noexcept
	  : _image(std::move(rhs._image))
	  , _mip_count(std::move(rhs._mip_count))
	  , _dimensions(std::move(rhs._dimensions))
	  , _transfer_task(std::move(rhs._transfer_task))
	{
	}

	Static_image& Static_image::operator=(Static_image&& rhs) noexcept
	{
		_image         = std::move(rhs._image);
		_mip_count     = std::move(rhs._mip_count);
		_dimensions    = std::move(rhs._dimensions);
		_transfer_task = std::move(rhs._transfer_task);

		return *this;
	}

	void Dynamic_buffer::update(const Command_buffer& cb, std::int32_t offset, gsl::span<const char> data)
	{
		MIRRAGE_INVARIANT(_capacity >= gsl::narrow<std::int32_t>(data.size() + offset), "Buffer overflow");
		MIRRAGE_INVARIANT(data.size() % 4 == 0, "buffer size has to be a multiple of 4: " << data.size());
		MIRRAGE_INVARIANT(offset % 4 == 0, "buffer offset has to be a multiple of 4: " << offset);

		auto buffer_barrier_pre = vk::BufferMemoryBarrier{_latest_usage_access,
		                                                  vk::AccessFlagBits::eTransferWrite,
		                                                  0,
		                                                  0,
		                                                  *_buffer,
		                                                  gsl::narrow<std::uint32_t>(offset),
		                                                  gsl::narrow<std::uint32_t>(data.size())};
		cb.pipelineBarrier(_latest_usage,
		                   vk::PipelineStageFlagBits::eTransfer,
		                   vk::DependencyFlags{},
		                   {},
		                   {buffer_barrier_pre},
		                   {});

		cb.updateBuffer(*_buffer,
		                gsl::narrow<std::uint32_t>(offset),
		                gsl::narrow<std::uint32_t>(data.size()),
		                data.data());

		auto buffer_barrier_post = vk::BufferMemoryBarrier{vk::AccessFlagBits::eTransferWrite,
		                                                   _earliest_usage_access,
		                                                   0,
		                                                   0,
		                                                   *_buffer,
		                                                   gsl::narrow<std::uint32_t>(offset),
		                                                   gsl::narrow<std::uint32_t>(data.size())};
		cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
		                   _earliest_usage,
		                   vk::DependencyFlags{},
		                   {},
		                   {buffer_barrier_post},
		                   {});
	}
	void Dynamic_buffer::_pre_update(const Command_buffer& cb)
	{
		auto buffer_barrier_pre = vk::BufferMemoryBarrier{
		        _latest_usage_access, vk::AccessFlagBits::eTransferWrite, 0, 0, *_buffer, 0, VK_WHOLE_SIZE};
		cb.pipelineBarrier(_latest_usage,
		                   vk::PipelineStageFlagBits::eTransfer,
		                   vk::DependencyFlags{},
		                   {},
		                   {buffer_barrier_pre},
		                   {});
	}

	void Dynamic_buffer::_do_update(const Command_buffer& cb, std::int32_t offset, gsl::span<const char> data)
	{
		MIRRAGE_INVARIANT(_capacity >= gsl::narrow<std::int32_t>(data.size() + offset), "Buffer overflow");
		MIRRAGE_INVARIANT(data.size() % 4 == 0, "buffer size has to be a multiple of 4: " << data.size());
		MIRRAGE_INVARIANT(offset % 4 == 0, "buffer offset has to be a multiple of 4: " << offset);

		cb.updateBuffer(*_buffer,
		                gsl::narrow<std::uint32_t>(offset),
		                gsl::narrow<std::uint32_t>(data.size()),
		                data.data());
	}

	void Dynamic_buffer::_post_update(const Command_buffer& cb)
	{
		auto buffer_barrier_post = vk::BufferMemoryBarrier{
		        vk::AccessFlagBits::eTransferWrite, _earliest_usage_access, 0, 0, *_buffer, 0, VK_WHOLE_SIZE};
		cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
		                   _earliest_usage,
		                   vk::DependencyFlags{},
		                   {},
		                   {buffer_barrier_post},
		                   {});
	}


	Transfer_manager::Transfer_manager(Device& device, Queue_tag transfer_queue, std::size_t max_frames)
	  : _device(device)
	  , _queue_family(_device.get_queue_family(transfer_queue))
	  , _queue(_device.get_queue(transfer_queue))
	  , _semaphore(device.create_semaphore())
	  , _command_buffer_pool(device.create_command_buffer_pool(transfer_queue, true, true))
	  , _command_buffers(
	            device,
	            +[](vk::UniqueCommandBuffer& cb) { cb->reset({}); },
	            [&] { return std::move(_command_buffer_pool.create_primary()[0]); },
	            max_frames)
	{

		if(!_device.is_unified_memory_architecture()) {
			_buffer_transfers.reserve(128);
		}

		_image_transfers.reserve(128);

		_reset_transfer_event();
	}
	Transfer_manager::~Transfer_manager() {}

	auto Transfer_manager::upload_image(vk::ImageType,
	                                    std::uint32_t                             owner,
	                                    const Image_dimensions&                   dimensions,
	                                    vk::Format                                format,
	                                    std::int32_t                              mip_levels,
	                                    std::int32_t                              size,
	                                    std::function<void(char*, std::uint32_t)> write_data,
	                                    bool                                      dedicated) -> Static_image
	{

		auto stored_mip_levels = std::max(1, mip_levels);
		auto actual_mip_levels = mip_levels;
		if(actual_mip_levels == 0)
			actual_mip_levels = static_cast<std::int32_t>(
			        std::floor(std::log2(std::min(dimensions.width, dimensions.height))) + 1);

		// create staging buffer containing data
		auto staging_buffer = _device.create_buffer(
		        vk::BufferCreateInfo(
		                {}, gsl::narrow<std::uint32_t>(size), vk::BufferUsageFlagBits::eTransferSrc),
		        true,
		        Memory_lifetime::temporary);

		// fill buffer
		auto ptr = staging_buffer.memory().mapped_addr().get_or_throw("Staging GPU memory is not mapped!");
		auto begin_ptr = ptr;

		// extract sizes of mip levels
		auto mip_image_sizes = std::vector<std::uint32_t>();
		mip_image_sizes.reserve(gsl::narrow<std::size_t>(stored_mip_levels));
		for(auto i : util::range(stored_mip_levels)) {
			(void) i;

			MIRRAGE_INVARIANT(ptr - begin_ptr < size, "buffer overflow");

			auto mip_size = std::uint32_t(0);
			write_data(reinterpret_cast<char*>(&mip_size), sizeof(mip_size));
			write_data(ptr, mip_size);
			ptr += mip_size;

			mip_image_sizes.emplace_back(mip_size);
		}

		auto real_dimensions = Image_dimensions{std::max(1, dimensions.width),
		                                        std::max(1, dimensions.height),
		                                        std::max(1, dimensions.depth),
		                                        std::max(1, dimensions.layers)};

		auto image_create_info =
		        vk::ImageCreateInfo{vk::ImageCreateFlags{},
		                            vk::ImageType::e2D,
		                            format,
		                            vk::Extent3D(gsl::narrow<std::uint32_t>(real_dimensions.width),
		                                         gsl::narrow<std::uint32_t>(real_dimensions.height),
		                                         gsl::narrow<std::uint32_t>(real_dimensions.depth)),
		                            gsl::narrow<std::uint32_t>(actual_mip_levels),
		                            gsl::narrow<std::uint32_t>(real_dimensions.layers),
		                            vk::SampleCountFlagBits::e1,
		                            vk::ImageTiling::eOptimal,
		                            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst
		                                    | vk::ImageUsageFlagBits::eTransferSrc};

		auto final_image = _device.create_image(image_create_info, false, Memory_lifetime::normal, dedicated);

		{
			auto lock = std::scoped_lock{_mutex};
			_image_transfers.emplace_back(std::move(staging_buffer),
			                              *final_image,
			                              size,
			                              owner,
			                              actual_mip_levels,
			                              stored_mip_levels,
			                              mip_levels == 0,
			                              real_dimensions,
			                              std::move(mip_image_sizes));
		}

		return {std::move(final_image),
		        actual_mip_levels,
		        mip_levels == 0,
		        real_dimensions,
		        _transfer_done_task};
	}

	auto Transfer_manager::upload_buffer(vk::BufferUsageFlags       usage,
	                                     std::uint32_t              owner,
	                                     std::int32_t               size,
	                                     std::function<void(char*)> write_data,
	                                     bool                       dedicated) -> Static_buffer
	{

		auto staging_buffer_usage = _device.is_unified_memory_architecture()
		                                    ? usage | vk::BufferUsageFlagBits::eTransferDst
		                                    : vk::BufferUsageFlagBits::eTransferSrc;

		auto staging_buffer_lifetime = _device.is_unified_memory_architecture() ? Memory_lifetime::normal
		                                                                        : Memory_lifetime::temporary;

		auto staging_buffer =
		        _create_staging_buffer(staging_buffer_usage, staging_buffer_lifetime, size, write_data);

		if(_device.is_unified_memory_architecture()) {
			// no transfer required
			return {std::move(staging_buffer)};
		}


		auto final_buffer = _device.create_buffer(
		        vk::BufferCreateInfo(
		                {}, gsl::narrow<std::uint32_t>(size), usage | vk::BufferUsageFlagBits::eTransferDst),
		        false,
		        Memory_lifetime::normal,
		        dedicated);

		{
			auto lock = std::scoped_lock{_mutex};
			_buffer_transfers.emplace_back(std::move(staging_buffer), *final_buffer, size, owner);
		}

		return {std::move(final_buffer), _transfer_done_task};
	}

	auto Transfer_manager::_create_staging_buffer(vk::BufferUsageFlags       usage,
	                                              Memory_lifetime            lifetime,
	                                              std::int32_t               size,
	                                              std::function<void(char*)> write_data) -> Backed_buffer
	{

		auto staging_buffer = _device.create_buffer(
		        vk::BufferCreateInfo({}, gsl::narrow<std::uint32_t>(size), usage), true, lifetime);

		// fill buffer
		auto ptr = staging_buffer.memory().mapped_addr().get_or_throw("Staging GPU memory is not mapped!");

		write_data(ptr);

		return staging_buffer;
	}


	auto Transfer_manager::create_dynamic_buffer(std::int32_t           size,
	                                             vk::BufferUsageFlags   usage,
	                                             vk::PipelineStageFlags earliest_usage,
	                                             vk::AccessFlags        earliest_usage_access,
	                                             vk::PipelineStageFlags latest_usage,
	                                             vk::AccessFlags        latest_usage_access) -> Dynamic_buffer
	{
		auto create_info = vk::BufferCreateInfo(
		        {}, gsl::narrow<std::uint32_t>(size), usage | vk::BufferUsageFlagBits::eTransferDst);
		return {_device.create_buffer(create_info, false),
		        size,
		        earliest_usage,
		        earliest_usage_access,
		        latest_usage,
		        latest_usage_access};
	}

	auto Transfer_manager::next_frame(vk::CommandBuffer main_queue_commands) -> util::maybe<vk::Semaphore>
	{
		auto lock = std::scoped_lock{_mutex};

		if(_buffer_transfers.empty() && _image_transfers.empty())
			return util::nothing;

		main_queue_commands.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});


		auto& command_buffer = *_command_buffers.current();

		command_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

		for(auto& t : _buffer_transfers) {
			// barrier against Host-Write (TODO: shouldn't be necessary)
			auto src_barrier = vk::BufferMemoryBarrier{vk::AccessFlagBits::eHostWrite,
			                                           vk::AccessFlagBits::eTransferRead,
			                                           VK_QUEUE_FAMILY_IGNORED,
			                                           VK_QUEUE_FAMILY_IGNORED,
			                                           *t.src,
			                                           0,
			                                           VK_WHOLE_SIZE};
			command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost,
			                               vk::PipelineStageFlagBits::eTransfer,
			                               vk::DependencyFlags{},
			                               {},
			                               {src_barrier},
			                               {});

			command_buffer.copyBuffer(*t.src, t.dst, {vk::BufferCopy{0, 0, t.size}});

			//  queue family release operation
			auto buffer_barrier = vk::BufferMemoryBarrier{
			        vk::AccessFlagBits::eTransferWrite,
			        vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eMemoryRead
			                | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eVertexAttributeRead,
			        _queue_family,
			        t.owner,
			        t.dst,
			        0,
			        VK_WHOLE_SIZE};
			// dstStageMask should be 0 (based on the spec) but validation layer complains
			command_buffer.pipelineBarrier(
			        vk::PipelineStageFlagBits::eTransfer,
			        vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader
			                | vk::PipelineStageFlagBits::eVertexShader
			                | vk::PipelineStageFlagBits::eVertexInput | vk::PipelineStageFlagBits::eTransfer,
			        vk::DependencyFlags{},
			        {},
			        {buffer_barrier},
			        {});

			_device.destroy_after_frame(std::move(t.src));

			// executed in graphics queue
			//  queue family aquire operation
			auto aq_buffer_barrier = vk::BufferMemoryBarrier{
			        vk::AccessFlagBits::eTransferWrite,
			        vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eMemoryRead
			                | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eVertexAttributeRead,
			        _queue_family,
			        t.owner,
			        t.dst,
			        0,
			        VK_WHOLE_SIZE};
			// srcStageMask should be 0 (based on the spec) but validation layer complains
			main_queue_commands.pipelineBarrier(
			        vk::PipelineStageFlagBits::eTransfer,
			        vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader
			                | vk::PipelineStageFlagBits::eVertexShader
			                | vk::PipelineStageFlagBits::eVertexInput | vk::PipelineStageFlagBits::eTransfer,
			        vk::DependencyFlags{},
			        {},
			        {aq_buffer_barrier},
			        {});
		}
		_buffer_transfers.clear();

		for(auto& t : _image_transfers) {
			_transfer_image(command_buffer, t);

			//  queue family release operation
			auto subresource = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,
			                                             0,
			                                             gsl::narrow<std::uint32_t>(t.mip_count_actual),
			                                             0,
			                                             gsl::narrow<std::uint32_t>(t.dimensions.layers)};

			auto barrier =
			        vk::ImageMemoryBarrier{vk::AccessFlagBits::eTransferWrite,
			                               vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eShaderRead
			                                       | vk::AccessFlagBits::eTransferWrite,
			                               vk::ImageLayout::eTransferDstOptimal,
			                               vk::ImageLayout::eTransferDstOptimal,
			                               _queue_family,
			                               t.owner,
			                               t.dst,
			                               subresource};
			command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
			                               vk::PipelineStageFlagBits::eTransfer
			                                       | vk::PipelineStageFlagBits::eFragmentShader
			                                       | vk::PipelineStageFlagBits::eComputeShader,
			                               vk::DependencyFlags{},
			                               {},
			                               {},
			                               {barrier});

			_device.destroy_after_frame(std::move(t.src));


			// executed in graphics queue
			{
				// queue family aquire operation
				// srcStageMask should be 0 (based on the spec) but validation layer complains
				main_queue_commands.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
				                                    vk::PipelineStageFlagBits::eTransfer
				                                            | vk::PipelineStageFlagBits::eFragmentShader
				                                            | vk::PipelineStageFlagBits::eComputeShader,
				                                    vk::DependencyFlags{},
				                                    {},
				                                    {},
				                                    {barrier});

				if(t.generate_mips) {
					// generate complete mip chain
					generate_mipmaps(main_queue_commands,
					                 t.dst,
					                 vk::ImageLayout::eTransferDstOptimal,
					                 vk::ImageLayout::eShaderReadOnlyOptimal,
					                 t.dimensions.width,
					                 t.dimensions.height,
					                 t.mip_count_actual);
				} else {
					image_layout_transition(main_queue_commands,
					                        t.dst,
					                        vk::ImageLayout::eTransferDstOptimal,
					                        vk::ImageLayout::eShaderReadOnlyOptimal,
					                        vk::ImageAspectFlagBits::eColor,
					                        0,
					                        t.mip_count_actual);
				}
			}
		}
		_image_transfers.clear();

		command_buffer.end();

		main_queue_commands.end();

		auto submit_info = vk::SubmitInfo{0, nullptr, nullptr, 1, &command_buffer, 1, &*_semaphore};
		_queue.submit({submit_info}, _command_buffers.start_new_frame().pass_to_queue());

		// signal waiting tasks that the transfer will be done in this frame
		_tranfer_done_event.set();
		_reset_transfer_event();

		return *_semaphore;
	}


	void Transfer_manager::_transfer_image(vk::CommandBuffer cb, const Transfer_image_req& t)
	{
		image_layout_transition(cb,
		                        t.dst,
		                        vk::ImageLayout::eUndefined,
		                        vk::ImageLayout::eTransferDstOptimal,
		                        vk::ImageAspectFlagBits::eColor,
		                        0,
		                        t.mip_count_actual);

		auto regions = std::vector<vk::BufferImageCopy>();
		regions.reserve(gsl::narrow<std::size_t>(t.mip_count_loaded));
		auto offset = std::uint32_t(0);

		for(auto i : util::range(t.mip_count_loaded)) {
			auto size = t.mip_image_sizes[std::size_t(i)];

			MIRRAGE_INVARIANT(offset + size <= t.size, "Overflow in _transfer_image");

			auto subresource = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor,
			                                              gsl::narrow<std::uint32_t>(i),
			                                              0,
			                                              gsl::narrow<std::uint32_t>(t.dimensions.layers)};
			auto extent      = vk::Extent3D{gsl::narrow<std::uint32_t>(std::max(1, t.dimensions.width >> i)),
                                       gsl::narrow<std::uint32_t>(std::max(1, t.dimensions.height >> i)),
                                       gsl::narrow<std::uint32_t>(std::max(1, t.dimensions.depth >> i))};
			regions.emplace_back(offset, 0, 0, subresource, vk::Offset3D{}, extent);
			offset += size;
		}

		cb.copyBufferToImage(*t.src, t.dst, vk::ImageLayout::eTransferDstOptimal, regions);
	}

	void Transfer_manager::_reset_transfer_event()
	{
		_tranfer_done_event = async::event_task<void>{};
		_transfer_done_task = _tranfer_done_event.get_task().share();
	}

} // namespace mirrage::graphic
