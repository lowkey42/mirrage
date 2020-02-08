#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/graphic/streamed_buffer.hpp>


namespace mirrage::renderer {

	class Transparent_pass_factory;

	class Transparent_pass : public Render_pass {
	  public:
		using Factory = Transparent_pass_factory;

		Transparent_pass(Deferred_renderer&, ecs::Entity_manager& ecs, graphic::Render_target_2D& target);

		// TODO
		//void handle_obj(Frame_data&, Culling_mask, ecs::Entity_facet, Transform_comp&, Model_comp&, Sub_mesh&);
		//void handle_obj(Frame_data&,
		//                Culling_mask,
		//                ecs::Entity_facet,
		//                Transform_comp&,
		//                Model_comp&,
		//                Skinning_type,
		//                std::int32_t pose_offset);
		void handle_obj(Frame_data&, Culling_mask, Particle_system_comp&, Particle_emitter&);

		void pre_draw(Frame_data&);
		void post_draw(Frame_data&);

		auto name() const noexcept -> const char* override { return "Transparent"; }

	  private:
		struct Stage_data {
			graphic::Command_pool_group    group;
			graphic::Render_pass_stage_ref stage;
		};

		ecs::Entity_manager&      _ecs;
		vk::Format                _revealage_format;
		graphic::Render_target_2D _accum;
		graphic::Render_target_2D _revealage;

		vk::UniqueSampler                   _sampler;
		vk::UniqueDescriptorSetLayout       _desc_set_layout;
		std::vector<graphic::DescriptorSet> _accum_descriptor_sets;
		std::vector<graphic::DescriptorSet> _compose_descriptor_sets;
		std::vector<graphic::DescriptorSet> _upsample_descriptor_sets;

		vk::UniqueSampler                    _depth_sampler;
		graphic::Image_descriptor_set_layout _mip_desc_set_layout;
		std::vector<graphic::DescriptorSet>  _mip_descriptor_sets;

		std::vector<graphic::Framebuffer> _mip_framebuffers;
		graphic::Render_pass              _mip_render_pass;

		std::vector<graphic::Framebuffer> _accum_framebuffers;
		graphic::Render_pass              _accum_render_pass;

		std::vector<graphic::Framebuffer> _upsample_framebuffers;
		graphic::Render_pass              _upsample_render_pass;

		graphic::Framebuffer _compose_framebuffer;
		graphic::Render_pass _compose_render_pass;

		graphic::Dynamic_buffer _light_uniforms;
		std::vector<char>       _light_uniforms_tmp;

		Stage_data _stages_particle_lit;   // [render_pass 1, subpass 0]
		Stage_data _stages_particle_unlit; // [render_pass 1, subpass 0]

		// TODO:
		// model_static N		[render_pass 2, subpass 0]
		// model_anim N			[render_pass 2, subpass 1]
		// billboards			[render_pass 2, subpass 2]

		auto _get_cmd_buffer(Frame_data& frame, Stage_data& stage_ref) -> std::pair<vk::CommandBuffer, bool>;
	};

	class Transparent_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Transparent_pass_factory>();
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
