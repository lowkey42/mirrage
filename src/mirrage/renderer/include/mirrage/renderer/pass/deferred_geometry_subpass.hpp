#pragma once

#include <mirrage/renderer/animation.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/streamed_buffer.hpp>

#include <tsl/robin_map.h>
#include <gsl/gsl>


namespace mirrage::graphic {
	struct Pipeline_description;
	class Subpass_builder;
} // namespace mirrage::graphic

namespace mirrage::renderer {
	class Pose_comp;

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

		void update(util::Time dt);
		void pre_draw(Frame_data&);
		void draw(Frame_data&, graphic::Render_pass&);

	  private:
		ecs::Entity_manager& _ecs;
		Deferred_renderer&   _renderer;

		Model _particle_billboard;

		util::iter_range<std::vector<Geometry>::iterator> _geometry_range;
		util::iter_range<std::vector<Geometry>::iterator> _rigged_geometry_range;

		vk::UniqueDescriptorSetLayout _decal_input_attachment_descriptor_set_layout;
		graphic::DescriptorSet        _decal_input_attachment_descriptor_set;
	};
} // namespace mirrage::renderer
