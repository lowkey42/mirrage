#include <mirrage/graphic/vk_wrapper.hpp>

#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/texture.hpp>


namespace mirrage::graphic {

	Command_buffer_pool::Command_buffer_pool(const vk::Device& device, vk::UniqueCommandPool pool)
	  : _device(device), _pool(std::move(pool))
	{
	}
	Command_buffer_pool::~Command_buffer_pool() { _device.waitIdle(); }

	auto Command_buffer_pool::create_primary(std::size_t count) -> std::vector<vk::UniqueCommandBuffer>
	{
		return _device.allocateCommandBuffersUnique(
		        vk::CommandBufferAllocateInfo(*_pool, vk::CommandBufferLevel::ePrimary, count));
	}
	auto Command_buffer_pool::create_secondary(std::size_t count) -> std::vector<vk::UniqueCommandBuffer>
	{
		return _device.allocateCommandBuffersUnique(
		        vk::CommandBufferAllocateInfo(*_pool, vk::CommandBufferLevel::eSecondary, count));
	}


	Fence::operator bool() const { return _device.getFenceStatus(*_fence) == vk::Result::eSuccess; }
	void   Fence::reset() { _device.resetFences({*_fence}); }
	void Fence::wait() { _device.waitForFences({*_fence}, true, std::numeric_limits<std::uint64_t>::max()); }

	Fence::Fence(const vk::Device& device) : _device(device), _fence(_device.createFenceUnique({})) {}

	auto create_fence(Device& d) -> Fence { return d.create_fence(); }



	Framebuffer::Framebuffer(vk::UniqueFramebuffer       fb,
	                         vk::Viewport                viewport,
	                         vk::Rect2D                  scissor,
	                         std::vector<vk::ClearValue> cv)
	  : _fb(std::move(fb)), _viewport(viewport), _scissor(scissor), _clear_values(std::move(cv))
	{
	}

	void Framebuffer::viewport(float x, float y, float width, float height, float min_depth, float max_depth)
	{
		_viewport.x        = x;
		_viewport.y        = y;
		_viewport.width    = width;
		_viewport.height   = height;
		_viewport.minDepth = min_depth;
		_viewport.maxDepth = max_depth;

		_scissor.offset.x      = x;
		_scissor.offset.y      = y;
		_scissor.extent.width  = width;
		_scissor.extent.height = height;
	}


	namespace {
		auto get_access_mask(vk::ImageLayout layout) -> vk::AccessFlags
		{
			switch(layout) {
				case vk::ImageLayout::eUndefined: return vk::AccessFlags{};

				case vk::ImageLayout::eGeneral: return ~vk::AccessFlags{};

				case vk::ImageLayout::ePreinitialized: return vk::AccessFlagBits::eHostWrite;

				case vk::ImageLayout::eColorAttachmentOptimal:
					return vk::AccessFlagBits::eColorAttachmentWrite;

				case vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimalKHR:
				case vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimalKHR:
				case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
					return vk::AccessFlagBits::eDepthStencilAttachmentRead;
					return vk::AccessFlagBits::eDepthStencilAttachmentRead;

				case vk::ImageLayout::eDepthStencilAttachmentOptimal:
					return vk::AccessFlagBits::eDepthStencilAttachmentRead
					       | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

				case vk::ImageLayout::eTransferSrcOptimal: return vk::AccessFlagBits::eTransferRead;

				case vk::ImageLayout::eTransferDstOptimal: return vk::AccessFlagBits::eTransferWrite;

				case vk::ImageLayout::eShaderReadOnlyOptimal: return vk::AccessFlagBits::eShaderRead;

				case vk::ImageLayout::eSharedPresentKHR:
				case vk::ImageLayout::ePresentSrcKHR: return vk::AccessFlagBits::eColorAttachmentWrite;
			}
			MIRRAGE_FAIL("Unreachable");
		}
	} // namespace

	void image_layout_transition(vk::CommandBuffer    cb,
	                             vk::Image            image,
	                             vk::ImageLayout      src_layout,
	                             vk::ImageLayout      dst_layout,
	                             vk::ImageAspectFlags aspects,
	                             std::uint32_t        mip_level,
	                             std::uint32_t        mip_level_count)
	{

		auto subresource = vk::ImageSubresourceRange{aspects, mip_level, mip_level_count, 0, 1};

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
	                      std::uint32_t     width,
	                      std::uint32_t     height,
	                      std::uint32_t     mip_count,
	                      std::uint32_t     start_mip_level,
	                      bool              filter_linear)
	{

		if(mip_count == 0) {
			mip_count = std::floor(std::log2(std::min(width, height))) + 1;
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
			        vk::Offset3D{0, 0, 0},
			        vk::Offset3D{int32_t(width >> (level - 1)), int32_t(height >> (level - 1)), 1}};
			auto dst_range = std::array<vk::Offset3D, 2>{
			        vk::Offset3D{0, 0, 0},
			        vk::Offset3D{int32_t(width >> level), int32_t(height >> level), 1}};

			auto blit = vk::ImageBlit{
			        // src
			        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, level - 1, 0, 1},
			        src_range,

			        // dst
			        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, level, 0, 1},
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
	                  vk::ImageLayout             final_dst_layout)
	{

		if(initial_src_layout != vk::ImageLayout::eTransferSrcOptimal) {
			image_layout_transition(cb,
			                        src.image(),
			                        initial_src_layout,
			                        vk::ImageLayout::eTransferSrcOptimal,
			                        vk::ImageAspectFlagBits::eColor,
			                        0,
			                        1);
		}

		if(initial_dst_layout != vk::ImageLayout::eTransferDstOptimal) {
			image_layout_transition(cb,
			                        dst.image(),
			                        initial_dst_layout,
			                        vk::ImageLayout::eTransferDstOptimal,
			                        vk::ImageAspectFlagBits::eColor,
			                        0,
			                        1);
		}

		auto src_range = std::array<vk::Offset3D, 2>{
		        vk::Offset3D{0, 0, 0}, vk::Offset3D{int32_t(src.width()), int32_t(src.height()), 1}};
		auto dst_range = std::array<vk::Offset3D, 2>{
		        vk::Offset3D{0, 0, 0}, vk::Offset3D{int32_t(dst.width()), int32_t(dst.height()), 1}};
		auto blit = vk::ImageBlit{// src
		                          vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
		                          src_range,

		                          // dst
		                          vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
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
			                        0,
			                        1);
		}

		if(final_dst_layout != vk::ImageLayout::eTransferDstOptimal) {
			image_layout_transition(cb,
			                        dst.image(),
			                        vk::ImageLayout::eTransferDstOptimal,
			                        final_dst_layout,
			                        vk::ImageAspectFlagBits::eColor,
			                        0,
			                        1);
		}
	}

	void clear_texture(vk::CommandBuffer           cb,
	                   const detail::Base_texture& img,
	                   util::Rgba                  color,
	                   vk::ImageLayout             initial_layout,
	                   vk::ImageLayout             final_layout,
	                   std::uint32_t               initial_mip_level,
	                   std::uint32_t               mip_levels)
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
	                   std::uint32_t     width,
	                   std::uint32_t     height,
	                   util::Rgba        color,
	                   vk::ImageLayout   initial_layout,
	                   vk::ImageLayout   final_layout,
	                   std::uint32_t     initial_mip_level,
	                   std::uint32_t     mip_levels)
	{

		if(mip_levels == 0) {
			mip_levels = std::floor(std::log2(std::min(width, height))) + 1;
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
		                   {vk::ImageSubresourceRange{
		                           vk::ImageAspectFlagBits::eColor, initial_mip_level, mip_levels, 0, 1}});

		graphic::image_layout_transition(cb,
		                                 img,
		                                 vk::ImageLayout::eTransferDstOptimal,
		                                 final_layout,
		                                 vk::ImageAspectFlagBits::eColor,
		                                 initial_mip_level,
		                                 mip_levels);
	}
} // namespace mirrage::graphic
