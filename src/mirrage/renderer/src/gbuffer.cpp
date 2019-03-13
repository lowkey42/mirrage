#include <mirrage/renderer/gbuffer.hpp>

#include <mirrage/graphic/device.hpp>

namespace mirrage::renderer {

	namespace {
		auto get_hdr_format(graphic::Device& device)
		{
			auto format =
			        device.get_supported_format({vk::Format::eR16G16B16A16Sfloat},
			                                    vk::FormatFeatureFlagBits::eColorAttachmentBlend
			                                            | vk::FormatFeatureFlagBits::eSampledImageFilterLinear
			                                            | vk::FormatFeatureFlagBits::eStorageImage
			                                            | vk::FormatFeatureFlagBits::eTransferDst
			                                            | vk::FormatFeatureFlagBits::eTransferSrc);

			if(format.is_some())
				return format.get_or_throw();

			LOG(plog::warning) << "HDR render targets are not supported! Falling back to LDR rendering!";

			return device.get_texture_rgb_format().get_or_throw("No rgb-format supported");
		}

		auto get_depth_format(graphic::Device& device)
		{
			auto format = device.get_supported_format(
			        {vk::Format::eR32Sfloat},
			        vk::FormatFeatureFlagBits::eColorAttachment
			                | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			if(format.is_some())
				return format.get_or_throw();

			LOG(plog::warning) << "Depth (R16 / R32) render targets are not supported! Falling back to LDR "
			                      "rendering!";

			return get_hdr_format(device);
		}
	} // namespace

	GBuffer::GBuffer(graphic::Device&          device,
	                 graphic::Descriptor_pool& desc_pool,
	                 std::int32_t              width,
	                 std::int32_t              height)
	  : mip_levels(static_cast<std::int32_t>(std::floor(std::log2(std::min(width, height))) - 2))
	  , depth_format(get_depth_format(device))
	  , depth(device,
	          {width, height},
	          mip_levels,
	          depth_format,
	          vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                  | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc
	                  | vk::ImageUsageFlagBits::eTransferDst,
	          vk::ImageAspectFlagBits::eColor)
	  , prev_depth(device,
	               {width, height},
	               1,
	               depth_format,
	               vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                       | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc
	                       | vk::ImageUsageFlagBits::eTransferDst,
	               vk::ImageAspectFlagBits::eColor)
	  , depth_buffer(device,
	                 {width, height},
	                 mip_levels,
	                 device.get_depth_format(),
	                 vk::ImageUsageFlagBits::eDepthStencilAttachment
	                         | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc
	                         | vk::ImageUsageFlagBits::eTransferDst,
	                 vk::ImageAspectFlagBits::eDepth)

	  , albedo_mat_id_format(device.get_texture_rgba_format().get_or_throw("No rgba-format supported"))
	  , albedo_mat_id(device,
	                  {width, height},
	                  mip_levels,
	                  albedo_mat_id_format,
	                  vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                          | vk::ImageUsageFlagBits::eInputAttachment
	                          | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
	                  vk::ImageAspectFlagBits::eColor)

	  , mat_data_format(device.get_texture_rgba_format().get_or_throw("No rgba-format supported"))
	  , mat_data(device,
	             {width, height},
	             mip_levels,
	             mat_data_format,
	             vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                     | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc
	                     | vk::ImageUsageFlagBits::eTransferDst,
	             vk::ImageAspectFlagBits::eColor)

	  , color_format(get_hdr_format(device))
	  , colorA(device,
	           {width, height},
	           mip_levels,
	           color_format,
	           vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                   | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc
	                   | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage,
	           vk::ImageAspectFlagBits::eColor)

	  , colorB(device,
	           {width, height},
	           mip_levels,
	           color_format,
	           vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                   | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc
	                   | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage,
	           vk::ImageAspectFlagBits::eColor)

	  , shadowmaps_layout(device.create_descriptor_set_layout(std::array<vk::DescriptorSetLayoutBinding, 3>{
	            vk::DescriptorSetLayoutBinding{0,
	                                           vk::DescriptorType::eSampledImage,
	                                           gsl::narrow<std::uint32_t>(max_shadowmaps),
	                                           vk::ShaderStageFlagBits::eFragment},
	            vk::DescriptorSetLayoutBinding{
	                    1, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment},
	            vk::DescriptorSetLayoutBinding{
	                    2, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment},
	    }))
	  , shadowmaps(desc_pool.create_descriptor(*shadowmaps_layout, 3))
	{
	}
} // namespace mirrage::renderer
