#pragma once

#include <mirrage/graphic/texture.hpp>


namespace mirrage::renderer {

	struct Renderer_settings;

	struct GBuffer {
	  public:
		GBuffer(graphic::Device&          device,
		        graphic::Descriptor_pool& desc_pool,
		        std::int32_t              width,
		        std::int32_t              height,
		        const Renderer_settings&  settings);

		std::int32_t mip_levels;

		bool depth_sampleable;

		vk::Format                depth_format;
		graphic::Render_target_2D depth;        // depth-buffer
		graphic::Render_target_2D prev_depth;   // depth-buffer from previouse frame
		graphic::Render_target_2D depth_buffer; // actual depth-buffer for Z-tests

		vk::Format                albedo_mat_id_format;
		graphic::Render_target_2D albedo_mat_id; // RGB of objects + Material-ID

		vk::Format                mat_data_format;
		graphic::Render_target_2D mat_data; // matId=0: 16Bit-Normals, Roughness, Metal

		vk::Format                color_format;
		graphic::Render_target_2D colorA; //< lighting result
		graphic::Render_target_2D colorB; //< for ping-pong rendering

		util::maybe<graphic::Texture_2D&> ambient_occlusion;

		vk::UniqueDescriptorSetLayout animation_data_layout;
		vk::DescriptorSet             animation_data; //< might change each frame!

		bool                          shadowmapping_enabled = false;
		std::int32_t                  max_shadowmaps        = 1;
		vk::UniqueDescriptorSetLayout shadowmaps_layout;
		graphic::DescriptorSet        shadowmaps;

		util::maybe<graphic::Texture_2D_array&> voxels;

		float min_luminance = 1e-4f;
		float max_luminance = 1e6f;
	};
} // namespace mirrage::renderer
