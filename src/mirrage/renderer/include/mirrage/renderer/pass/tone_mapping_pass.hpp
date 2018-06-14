#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Tone_mapping_pass : public Pass {
	  public:
		Tone_mapping_pass(Deferred_renderer&,
		                  ecs::Entity_manager&,
		                  util::maybe<Meta_system&>,
		                  graphic::Render_target_2D& src,
		                  graphic::Render_target_2D& target);


		void update(util::Time dt) override;
		void draw(vk::CommandBuffer&,
		          Command_buffer_source&,
		          vk::DescriptorSet global_uniform_set,
		          std::size_t       swapchain_image) override;

		auto last_histogram() const noexcept -> auto& { return _last_result_data; }
		auto max_histogram_size() const noexcept { return _last_max_histogram_size; }

		auto name() const noexcept -> const char* override { return "Tone Mapping"; }

	  private:
		Deferred_renderer&         _renderer;
		graphic::Render_target_2D& _src;
		graphic::Render_target_2D& _target;
		bool                       _first_frame = true;

		std::vector<graphic::Backed_buffer> _result_buffer;
		int                                 _ready_result = -1;
		int                                 _next_result  = 0;
		std::vector<float>                  _last_result_data;
		std::uint32_t                       _last_max_histogram_size = 0;

		vk::UniqueSampler                    _sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;
		vk::Format                           _luminance_format;

		// the histogram adjustment factor for each histogram bucket
		graphic::Render_target_2D _adjustment_buffer;

		vk::UniqueDescriptorSetLayout       _compute_descriptor_set_layout;
		std::vector<graphic::DescriptorSet> _compute_descriptor_set;

		// calculate histogram and tone mapping factors
		vk::UniquePipelineLayout _compute_pipeline_layout;
		vk::UniquePipeline       _build_histogram_pipeline;
		vk::UniquePipeline       _adjust_histogram_pipeline;

		// apply tone mapping
		graphic::Framebuffer   _apply_framebuffer;
		graphic::Render_pass   _apply_renderpass;
		graphic::DescriptorSet _apply_desc_set;

		void _clear_result_buffer(vk::CommandBuffer&);
		auto _generate_foveal_image(vk::CommandBuffer&) -> std::uint32_t;
		void _dispatch_build_histogram(vk::DescriptorSet, vk::CommandBuffer&, std::uint32_t mip_level);
		void _dispatch_adjust_histogram(vk::DescriptorSet, vk::CommandBuffer&, std::uint32_t mip_level);
		void _apply_tone_ampping(vk::DescriptorSet, vk::CommandBuffer&, std::uint32_t mip_level);
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
