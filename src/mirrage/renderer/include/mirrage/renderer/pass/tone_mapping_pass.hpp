#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Tone_mapping_pass_factory;

	class Tone_mapping_pass : public Render_pass {
	  public:
		using Factory = Tone_mapping_pass_factory;

		Tone_mapping_pass(Deferred_renderer&,
		                  graphic::Render_target_2D& src,
		                  graphic::Render_target_2D& target);


		void update(util::Time dt) override;
		void post_draw(Frame_data&);

		auto last_histogram() const noexcept -> auto& { return _last_result_data; }
		auto max_histogram_size() const noexcept { return _last_max_histogram_size; }

		auto name() const noexcept -> const char* override { return "Tone Mapping"; }

	  private:
		graphic::Render_target_2D& _src;
		graphic::Render_target_2D& _target;
		bool                       _first_frame = true;

		std::vector<graphic::Backed_buffer> _result_buffer;
		int                                 _ready_result = -1;
		int                                 _next_result  = 0;
		std::vector<float>                  _last_result_data;
		int                                 _last_max_histogram_size = 0;

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

		void _clear_result_buffer(vk::CommandBuffer);
		auto _generate_foveal_image(vk::CommandBuffer) -> int;
		void _dispatch_build_histogram(vk::DescriptorSet, vk::CommandBuffer, int mip_level);
		void _dispatch_adjust_histogram(vk::DescriptorSet, vk::CommandBuffer, int mip_level);
		void _apply_tone_ampping(vk::DescriptorSet, vk::CommandBuffer, int mip_level);
	};

	class Tone_mapping_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Tone_mapping_pass_factory>();
		}

		auto create_pass(Deferred_renderer&,
		                 std::shared_ptr<void>,
		                 util::maybe<ecs::Entity_manager&>,
		                 Engine&,
		                 bool&) -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
