#pragma once

#include <mirrage/renderer/animation.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/graphic/streamed_buffer.hpp>
#include <mirrage/graphic/thread_local_command_buffer_pool.hpp>

#include <tsl/robin_map.h>
#include <gsl/gsl>


namespace mirrage::graphic {
	struct Pipeline_description;
	class Subpass_builder;
} // namespace mirrage::graphic

namespace mirrage::renderer {
	class Pose_comp;
	class Decal_comp;
	class Model_comp;

	class Deferred_geometry_subpass {
	  public:
		Deferred_geometry_subpass(Deferred_renderer&, ecs::Entity_manager&);

		void configure_pipeline(Deferred_renderer&, graphic::Pipeline_description&);
		void configure_subpass(Deferred_renderer&, graphic::Subpass_builder&);
		void configure_emissive_subpass(Deferred_renderer&, graphic::Subpass_builder&);

		void configure_animation_pipeline(Deferred_renderer&, graphic::Pipeline_description&);
		void configure_animation_subpass(Deferred_renderer&, graphic::Subpass_builder&);
		void configure_animation_emissive_subpass(Deferred_renderer&, graphic::Subpass_builder&);

		void configure_billboard_pipeline(Deferred_renderer&, graphic::Pipeline_description&);
		void configure_billboard_subpass(Deferred_renderer&, graphic::Subpass_builder&);

		void configure_decal_pipeline(Deferred_renderer&, graphic::Pipeline_description&);
		void configure_decal_subpass(Deferred_renderer&, graphic::Subpass_builder&);

		void configure_particle_pipeline(Deferred_renderer&, graphic::Pipeline_description&);
		void configure_particle_subpass(Deferred_renderer&, graphic::Subpass_builder&);

		void on_render_pass_configured(graphic::Render_pass&, graphic::Framebuffer&);

		void update(util::Time dt);
		void draw(Frame_data&, graphic::Render_pass&);

		void handle_obj(Frame_data&,
		                Culling_mask,
		                ecs::Entity_facet,
		                const glm::vec4& emissive_color,
		                const glm::mat4& transform,
		                const Model&,
		                const Material_override&,
		                const Sub_mesh&);
		void handle_obj(Frame_data&,
		                Culling_mask,
		                ecs::Entity_facet,
		                const glm::vec4& emissive_color,
		                const glm::mat4& transform,
		                const Model&,
		                gsl::span<const Material_override>,
		                Skinning_type,
		                std::uint32_t pose_offset);

		void handle_obj(Frame_data&, Culling_mask, const Billboard&, const glm::vec3& pos = {});
		void handle_obj(Frame_data&, Culling_mask, const Decal&, const glm::mat4&);
		void handle_obj(Frame_data&,
		                Culling_mask,
		                const glm::vec4& emissive_color,
		                const Particle_system&,
		                const Particle_emitter&);

	  private:
		struct Stage_data {
			graphic::Command_pool_group    group;
			graphic::Render_pass_stage_ref stage;
		};
		using Stage_data_map = std::unordered_map<util::Str_id, Stage_data>;

		ecs::Entity_manager&               _ecs;
		Deferred_renderer&                 _renderer;
		util::maybe<graphic::Render_pass&> _render_pass;
		util::maybe<graphic::Framebuffer&> _framebuffer;

		vk::UniqueDescriptorSetLayout _decal_input_attachment_descriptor_set_layout;
		graphic::DescriptorSet        _decal_input_attachment_descriptor_set;

		Stage_data_map _stages_model_static;          // subpass: 0
		Stage_data_map _stages_model_anim;            // subpass: 1
		Stage_data_map _stages_model_static_emissive; // subpass: 2
		Stage_data_map _stages_model_anim_emissive;   // subpass: 3
		Stage_data     _stage_decals;                 // subpass: 4
		Stage_data     _stage_billboards;             // subpass: 5
		Stage_data     _stage_particles;              // subpass: 6

		auto _get_cmd_buffer(Frame_data& frame, Stage_data&) -> std::pair<vk::CommandBuffer, bool>;
	};
} // namespace mirrage::renderer
