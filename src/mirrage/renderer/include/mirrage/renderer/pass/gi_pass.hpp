#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Gi_pass : public Render_pass {
	  public:
		Gi_pass(Deferred_renderer&, graphic::Render_target_2D& in_out, graphic::Render_target_2D& diffuse_in);


		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "SSGI"; }

	  private:
		Deferred_renderer&                   _renderer;
		const std::uint32_t                  _highres_base_mip_level;
		const std::uint32_t                  _base_mip_level;
		const std::uint32_t                  _max_mip_level;
		const std::uint32_t                  _diffuse_mip_level;
		const std::uint32_t                  _min_mip_level;
		vk::UniqueSampler                    _gbuffer_sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;
		graphic::Render_target_2D&           _color_in_out;
		graphic::Render_target_2D&           _color_diffuse_in;
		bool                                 _first_frame = true;
		glm::mat4                            _prev_view;
		glm::mat4                            _prev_proj;
		glm::vec3                            _prev_eye_position;

		/// current GI result (diffuse only)
		///                     HISTORY  SAMPLES  RESULT
		/// reproject pass        out               in
		/// sample pass	                   out
		/// upsample pass                in/out     in (prev MIP)
		/// blend history pass    in        in     out
		graphic::Render_target_2D _gi_diffuse_history;
		graphic::Render_target_2D _gi_diffuse_samples;
		graphic::Render_target_2D _gi_diffuse_result;

		// current GI result + history buffer (specular only)
		graphic::Render_target_2D _gi_specular;
		graphic::Render_target_2D _gi_specular_history;

		vk::Format                _history_weight_format;
		graphic::Render_target_2D _history_weight;
		graphic::Render_target_2D _history_weight_prev;

		// preintegration of BRDF. Based on:
		//     http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
		//     https://learnopengl.com/#!PBR/IBL/Specular-IBL
		vk::Format                _integrated_brdf_format;
		graphic::Render_target_2D _integrated_brdf;
		graphic::Framebuffer      _brdf_integration_framebuffer;
		graphic::Render_pass      _brdf_integration_renderpass;

		// blend reprojected history into current current frame to simulate multiple bounces
		graphic::Framebuffer   _reproject_framebuffer;
		graphic::Render_pass   _reproject_renderpass;
		graphic::DescriptorSet _reproject_descriptor_set;

		// reproject higher diffuse MIP levels (for temporal smoothing of results)
		std::vector<graphic::Framebuffer> _diffuse_reproject_framebuffers;
		graphic::Render_pass              _diffuse_reproject_renderpass;

		// GI sampling for diffuse illumination
		std::vector<graphic::Framebuffer>   _sample_framebuffers;
		std::vector<graphic::Framebuffer>   _sample_framebuffers_blend;
		graphic::Render_pass                _sample_renderpass;
		std::vector<graphic::DescriptorSet> _sample_descriptor_sets;

		// SS cone tracing for specular illumination
		graphic::Framebuffer   _sample_spec_framebuffer;
		graphic::Render_pass   _sample_spec_renderpass;
		graphic::DescriptorSet _sample_spec_descriptor_set;

		// median filter to remove noise from specular illumination
		graphic::Framebuffer   _median_spec_framebuffer;
		graphic::Render_pass   _median_spec_renderpass;
		graphic::DescriptorSet _median_spec_descriptor_set;

		// blur pass for specular illumination
		graphic::Framebuffer   _blur_horizonal_framebuffer;
		graphic::Framebuffer   _blur_vertical_framebuffer;
		graphic::Render_pass   _blur_render_pass;
		graphic::DescriptorSet _blur_descriptor_set_horizontal;
		graphic::DescriptorSet _blur_descriptor_set_vertical;

		// write back GI results
		graphic::Framebuffer   _blend_framebuffer;
		graphic::Render_pass   _blend_renderpass;
		graphic::DescriptorSet _blend_descriptor_set;


		// calculates the texture with the preintegrated BRDF
		void _integrate_brdf(vk::CommandBuffer command_buffer);

		// blends _gi_diffuse_history into _color_diffuse_in to simulate multiple bounces
		void _reproject_history(vk::CommandBuffer command_buffer, vk::DescriptorSet globals);

		// generates mipmaps for _color_diffuse_in
		void _generate_first_mipmaps(vk::CommandBuffer command_buffer, vk::DescriptorSet globals);
		void _generate_mipmaps(vk::CommandBuffer command_buffer, vk::DescriptorSet globals);

		// calculates GI samples and stores them into the levels of _gi_diffuse and _gi_specular
		void _generate_gi_samples(vk::CommandBuffer command_buffer, vk::DescriptorSet globals);

		// blurs the specular samples
		void _blur_spec_gi(vk::CommandBuffer command_buffer);

		// blends the _gi_diffuse and _gi_specular into _color_in_out
		void _draw_gi(vk::CommandBuffer command_buffer);
	};

	class Gi_pass_factory : public Render_pass_factory {
	  public:
		auto create_pass(Deferred_renderer&, ecs::Entity_manager&, Engine&, bool&)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
