#pragma once

#include <mirrage/graphic/texture.hpp>


namespace mirrage::renderer {

	struct GBuffer {
	  public:
		GBuffer(graphic::Device& device, std::uint32_t width, std::uint32_t height);

		std::uint32_t mip_levels;

		vk::Format                depth_format;
		graphic::Render_target_2D depth;      // depth-buffer
		graphic::Render_target_2D prev_depth; // depth-buffer from previouse frame

		vk::Format                albedo_mat_id_format;
		graphic::Render_target_2D albedo_mat_id; // RGB of objects + Material-ID

		vk::Format                mat_data_format;
		graphic::Render_target_2D mat_data; // matId=0: 16Bit-Normals, Roughness, Metal

		vk::Format                color_format;
		graphic::Render_target_2D colorA; //< lighting result
		graphic::Render_target_2D colorB; //< for ping-pong rendering

		util::maybe<graphic::Texture_2D&> ambient_occlusion;

		vk::UniqueDescriptorSetLayout shadowmaps_layout;
		graphic::DescriptorSet        shadowmaps;

		util::maybe<graphic::Texture_2D_array&> voxels;

		util::maybe<graphic::Texture_2D&> avg_log_luminance;
		util::maybe<graphic::Texture_2D&> bloom;

		util::maybe<graphic::Texture_2D&> histogram_adjustment_factors;
		float                             min_luminance = 1e-4f;
		float                             max_luminance = 1e7f;
	};
} // namespace mirrage::renderer
