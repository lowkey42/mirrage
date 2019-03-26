#include <mirrage/graphic/vk_wrapper.hpp>

#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/texture.hpp>

#include <mirrage/utils/ranges.hpp>


namespace mirrage::graphic {

	Command_buffer_pool::Command_buffer_pool(const vk::Device& device, vk::UniqueCommandPool pool)
	  : _device(&device), _pool(std::move(pool))
	{
	}
	Command_buffer_pool::~Command_buffer_pool()
	{
		if(_device)
			_device->waitIdle();
	}

	auto Command_buffer_pool::create_primary(std::int32_t count) -> std::vector<vk::UniqueCommandBuffer>
	{
		return _device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(
		        *_pool, vk::CommandBufferLevel::ePrimary, gsl::narrow<std::uint32_t>(count)));
	}
	auto Command_buffer_pool::create_secondary(std::int32_t count) -> std::vector<vk::UniqueCommandBuffer>
	{
		return _device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(
		        *_pool, vk::CommandBufferLevel::eSecondary, gsl::narrow<std::uint32_t>(count)));
	}


	Fence::operator bool() const { return _device->getFenceStatus(*_fence) == vk::Result::eSuccess; }
	void   Fence::reset() { _device->resetFences({*_fence}); }
	void Fence::wait() { _device->waitForFences({*_fence}, true, std::numeric_limits<std::uint64_t>::max()); }

	Fence::Fence(const vk::Device& device, bool signaled)
	  : _device(&device)
	  , _fence(_device->createFenceUnique({signaled ? vk::FenceCreateFlags{vk::FenceCreateFlagBits::eSignaled}
	                                                : vk::FenceCreateFlags{}}))
	{
	}

	auto create_fence(Device& d) -> Fence { return d.create_fence(); }



	Framebuffer::Framebuffer(vk::UniqueFramebuffer       fb,
	                         vk::Viewport                viewport,
	                         vk::Rect2D                  scissor,
	                         std::vector<vk::ClearValue> cv)
	  : _fb(std::move(fb)), _viewport(viewport), _scissor(scissor), _clear_values(std::move(cv))
	{
	}

	void Framebuffer::viewport(std::int32_t x,
	                           std::int32_t y,
	                           std::int32_t width,
	                           std::int32_t height,
	                           float        min_depth,
	                           float        max_depth)
	{
		_viewport.x        = float(x);
		_viewport.y        = float(y);
		_viewport.width    = float(width);
		_viewport.height   = float(height);
		_viewport.minDepth = min_depth;
		_viewport.maxDepth = max_depth;

		_scissor.offset.x      = x;
		_scissor.offset.y      = y;
		_scissor.extent.width  = gsl::narrow<std::uint32_t>(width);
		_scissor.extent.height = gsl::narrow<std::uint32_t>(height);
	}


	namespace {
		auto get_access_mask(vk::ImageLayout layout) -> vk::AccessFlags
		{
			switch(layout) {
				case vk::ImageLayout::eUndefined: return vk::AccessFlags{};

				case vk::ImageLayout::ePreinitialized: return vk::AccessFlagBits::eHostWrite;

				case vk::ImageLayout::eColorAttachmentOptimal:
					return vk::AccessFlagBits::eColorAttachmentWrite;

				case vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimalKHR:
				case vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimalKHR:
				case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
					return vk::AccessFlagBits::eDepthStencilAttachmentRead;

				case vk::ImageLayout::eDepthStencilAttachmentOptimal:
					return vk::AccessFlagBits::eDepthStencilAttachmentRead
					       | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

				case vk::ImageLayout::eTransferSrcOptimal: return vk::AccessFlagBits::eTransferRead;

				case vk::ImageLayout::eTransferDstOptimal: return vk::AccessFlagBits::eTransferWrite;

				case vk::ImageLayout::eShaderReadOnlyOptimal: return vk::AccessFlagBits::eShaderRead;

				case vk::ImageLayout::eSharedPresentKHR:
				case vk::ImageLayout::ePresentSrcKHR: return vk::AccessFlagBits::eColorAttachmentWrite;

				default: return ~vk::AccessFlags{};
			}
		}
	} // namespace

	void image_layout_transition(vk::CommandBuffer    cb,
	                             vk::Image            image,
	                             vk::ImageLayout      src_layout,
	                             vk::ImageLayout      dst_layout,
	                             vk::ImageAspectFlags aspects,
	                             std::int32_t         mip_level,
	                             std::int32_t         mip_level_count)
	{

		auto subresource = vk::ImageSubresourceRange{aspects,
		                                             gsl::narrow<std::uint32_t>(mip_level),
		                                             gsl::narrow<std::uint32_t>(mip_level_count),
		                                             0,
		                                             1};

		auto barrier = vk::ImageMemoryBarrier{get_access_mask(src_layout),
		                                      get_access_mask(dst_layout),
		                                      src_layout,
		                                      dst_layout,
		                                      VK_QUEUE_FAMILY_IGNORED,
		                                      VK_QUEUE_FAMILY_IGNORED,
		                                      image,
		                                      subresource};
		cb.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
		                   vk::PipelineStageFlagBits::eAllCommands,
		                   vk::DependencyFlags{},
		                   {},
		                   {},
		                   {barrier});
	}

	// Generation of mip-chain
	//   based on: https://github.com/SaschaWillems/Vulkan/blob/master/texturemipmapgen/texturemipmapgen.cpp
	void generate_mipmaps(vk::CommandBuffer cb,
	                      vk::Image         image,
	                      vk::ImageLayout   src_layout,
	                      vk::ImageLayout   dst_layout,
	                      std::int32_t      width,
	                      std::int32_t      height,
	                      std::int32_t      mip_count,
	                      std::int32_t      start_mip_level,
	                      bool              filter_linear)
	{

		if(mip_count == 0) {
			mip_count = static_cast<std::int32_t>(std::floor(std::log2(std::min(width, height))) + 1);
		}

		if(src_layout != vk::ImageLayout::eTransferSrcOptimal) {
			image_layout_transition(cb,
			                        image,
			                        src_layout,
			                        vk::ImageLayout::eTransferSrcOptimal,
			                        vk::ImageAspectFlagBits::eColor,
			                        start_mip_level,
			                        1);
		}

		for(auto level : util::range(1 + start_mip_level, mip_count - 1)) {
			image_layout_transition(cb,
			                        image,
			                        vk::ImageLayout::eUndefined,
			                        vk::ImageLayout::eTransferDstOptimal,
			                        vk::ImageAspectFlagBits::eColor,
			                        level,
			                        1);

			auto src_range = std::array<vk::Offset3D, 2>{
			        vk::Offset3D{0, 0, 0}, vk::Offset3D{width >> (level - 1), height >> (level - 1), 1}};
			auto dst_range = std::array<vk::Offset3D, 2>{vk::Offset3D{0, 0, 0},
			                                             vk::Offset3D{width >> level, height >> level, 1}};

			auto blit = vk::ImageBlit{
			        // src
			        vk::ImageSubresourceLayers{
			                vk::ImageAspectFlagBits::eColor, gsl::narrow<std::uint32_t>(level - 1), 0, 1},
			        src_range,

			        // dst
			        vk::ImageSubresourceLayers{
			                vk::ImageAspectFlagBits::eColor, gsl::narrow<std::uint32_t>(level), 0, 1},
			        dst_range};
			cb.blitImage(image,
			             vk::ImageLayout::eTransferSrcOptimal,
			             image,
			             vk::ImageLayout::eTransferDstOptimal,
			             {blit},
			             filter_linear ? vk::Filter::eLinear : vk::Filter::eNearest);

			image_layout_transition(cb,
			                        image,
			                        vk::ImageLayout::eTransferDstOptimal,
			                        vk::ImageLayout::eTransferSrcOptimal,
			                        vk::ImageAspectFlagBits::eColor,
			                        level,
			                        1);
		}

		if(dst_layout != vk::ImageLayout::eTransferSrcOptimal) {
			image_layout_transition(cb,
			                        image,
			                        vk::ImageLayout::eTransferSrcOptimal,
			                        dst_layout,
			                        vk::ImageAspectFlagBits::eColor,
			                        start_mip_level,
			                        mip_count - start_mip_level);
		}
	}

	void blit_texture(vk::CommandBuffer           cb,
	                  const detail::Base_texture& src,
	                  vk::ImageLayout             initial_src_layout,
	                  vk::ImageLayout             final_src_layout,
	                  detail::Base_texture&       dst,
	                  vk::ImageLayout             initial_dst_layout,
	                  vk::ImageLayout             final_dst_layout,
	                  std::int32_t                src_mip,
	                  std::int32_t                dst_mip)
	{

		if(initial_src_layout != vk::ImageLayout::eTransferSrcOptimal) {
			image_layout_transition(cb,
			                        src.image(),
			                        initial_src_layout,
			                        vk::ImageLayout::eTransferSrcOptimal,
			                        vk::ImageAspectFlagBits::eColor,
			                        src_mip,
			                        1);
		}

		if(initial_dst_layout != vk::ImageLayout::eTransferDstOptimal) {
			image_layout_transition(cb,
			                        dst.image(),
			                        initial_dst_layout,
			                        vk::ImageLayout::eTransferDstOptimal,
			                        vk::ImageAspectFlagBits::eColor,
			                        dst_mip,
			                        1);
		}

		auto src_range = std::array<vk::Offset3D, 2>{
		        vk::Offset3D{0, 0, 0},
		        vk::Offset3D{int32_t(src.width(src_mip)), int32_t(src.height(src_mip)), 1}};
		auto dst_range = std::array<vk::Offset3D, 2>{
		        vk::Offset3D{0, 0, 0},
		        vk::Offset3D{int32_t(dst.width(dst_mip)), int32_t(dst.height(dst_mip)), 1}};
		auto blit = vk::ImageBlit{
		        // src
		        vk::ImageSubresourceLayers{
		                vk::ImageAspectFlagBits::eColor, gsl::narrow<std::uint32_t>(src_mip), 0, 1},
		        src_range,

		        // dst
		        vk::ImageSubresourceLayers{
		                vk::ImageAspectFlagBits::eColor, gsl::narrow<std::uint32_t>(dst_mip), 0, 1},
		        dst_range};

		cb.blitImage(src.image(),
		             vk::ImageLayout::eTransferSrcOptimal,
		             dst.image(),
		             vk::ImageLayout::eTransferDstOptimal,
		             {blit},
		             vk::Filter::eLinear);


		if(final_src_layout != vk::ImageLayout::eTransferSrcOptimal) {
			image_layout_transition(cb,
			                        src.image(),
			                        vk::ImageLayout::eTransferSrcOptimal,
			                        final_src_layout,
			                        vk::ImageAspectFlagBits::eColor,
			                        src_mip,
			                        1);
		}

		if(final_dst_layout != vk::ImageLayout::eTransferDstOptimal) {
			image_layout_transition(cb,
			                        dst.image(),
			                        vk::ImageLayout::eTransferDstOptimal,
			                        final_dst_layout,
			                        vk::ImageAspectFlagBits::eColor,
			                        dst_mip,
			                        1);
		}
	}

	void clear_texture(vk::CommandBuffer           cb,
	                   const detail::Base_texture& img,
	                   util::Rgba                  color,
	                   vk::ImageLayout             initial_layout,
	                   vk::ImageLayout             final_layout,
	                   std::int32_t                initial_mip_level,
	                   std::int32_t                mip_levels)
	{

		clear_texture(cb,
		              img.image(),
		              img.width(),
		              img.height(),
		              color,
		              initial_layout,
		              final_layout,
		              initial_mip_level,
		              mip_levels);
	}

	void clear_texture(vk::CommandBuffer cb,
	                   vk::Image         img,
	                   std::int32_t      width,
	                   std::int32_t      height,
	                   util::Rgba        color,
	                   vk::ImageLayout   initial_layout,
	                   vk::ImageLayout   final_layout,
	                   std::int32_t      initial_mip_level,
	                   std::int32_t      mip_levels)
	{

		if(mip_levels == 0) {
			mip_levels = gsl::narrow<std::int32_t>(std::floor(std::log2(std::min(width, height))) + 1);
		}

		graphic::image_layout_transition(cb,
		                                 img,
		                                 initial_layout,
		                                 vk::ImageLayout::eTransferDstOptimal,
		                                 vk::ImageAspectFlagBits::eColor,
		                                 initial_mip_level,
		                                 mip_levels);

		cb.clearColorImage(img,
		                   vk::ImageLayout::eTransferDstOptimal,
		                   vk::ClearColorValue{std::array<float, 4>{color.r, color.g, color.b, color.a}},
		                   {vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,
		                                              gsl::narrow<std::uint32_t>(initial_mip_level),
		                                              gsl::narrow<std::uint32_t>(mip_levels),
		                                              0,
		                                              1}});

		graphic::image_layout_transition(cb,
		                                 img,
		                                 vk::ImageLayout::eTransferDstOptimal,
		                                 final_layout,
		                                 vk::ImageAspectFlagBits::eColor,
		                                 initial_mip_level,
		                                 mip_levels);
	}
} // namespace mirrage::graphic
