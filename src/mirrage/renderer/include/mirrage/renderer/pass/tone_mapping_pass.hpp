#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Tone_mapping_pass : public Pass {
	  public:
		Tone_mapping_pass(Deferred_renderer&,
		                  ecs::Entity_manager&,
		                  util::maybe<Meta_system&>,
		                  graphic::Texture_2D& src);


		void update(util::Time dt) override;
		void draw(vk::CommandBuffer&,
		          Command_buffer_source&,
		          vk::DescriptorSet global_uniform_set,
		          std::size_t       swapchain_image) override;

		auto last_histogram() const noexcept -> auto& { return _last_result_data; }

		auto name() const noexcept -> const char* override { return "Tone Mapping"; }

	  private:
		Deferred_renderer&      _renderer;
		graphic::Texture_2D&    _src;
		graphic::Fence          _compute_fence;
		vk::UniqueCommandBuffer _last_compute_commands;
		bool                    _first_frame = true;

		std::vector<graphic::Backed_buffer> _result_buffer;
		int                                 _ready_result = -1;
		int                                 _next_result  = 0;
		std::vector<float>                  _last_result_data;

		vk::UniqueSampler                    _sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;
		vk::Format                           _luminance_format;

		// the histogram adjustment factor for each histogram bucket
		graphic::Render_target_2D _adjustment_buffer;

		vk::UniqueDescriptorSetLayout       _compute_descriptor_set_layout;
		std::vector<graphic::DescriptorSet> _compute_descriptor_set;

		// calculate scene luminance for tone mapping
		graphic::Render_target_2D _luminance_buffer;
		graphic::Framebuffer      _calc_luminance_framebuffer;
		graphic::Render_pass      _calc_luminance_renderpass;
		graphic::DescriptorSet    _calc_luminance_desc_set;

		vk::UniquePipelineLayout _compute_pipeline_layout;
		vk::UniquePipeline       _build_histogram_pipeline;
		vk::UniquePipeline       _compute_exposure_pipeline;
		vk::UniquePipeline       _adjust_histogram_pipeline;
		vk::UniquePipeline       _build_final_factors_pipeline;

		void _extract_luminance(vk::DescriptorSet, vk::CommandBuffer&);
		void _dispatch_build_histogram(vk::DescriptorSet, vk::CommandBuffer&);
		void _dispatch_compute_exposure(vk::DescriptorSet, vk::CommandBuffer&);
		void _dispatch_adjust_histogram(vk::DescriptorSet, vk::CommandBuffer&);
		void _dispatch_build_final_factors(vk::DescriptorSet, vk::CommandBuffer&);
	};

	class Tone_mapping_pass_factory : public Pass_factory {
	  public:
		auto create_pass(Deferred_renderer&,
		                 ecs::Entity_manager&,
		                 util::maybe<Meta_system&>,
		                 bool& write_first_pp_buffer) -> std::unique_ptr<Pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue, int current_score)
		        -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t> graphics_queue,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
