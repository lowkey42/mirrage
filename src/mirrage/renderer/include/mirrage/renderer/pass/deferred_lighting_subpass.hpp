#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/light_comp.hpp>


namespace mirrage::graphic {
	struct Pipeline_description;
	class Subpass_builder;
} // namespace mirrage::graphic

namespace mirrage::renderer {

	class Deferred_lighting_subpass {
	  public:
		Deferred_lighting_subpass(Deferred_renderer&, ecs::Entity_manager&, graphic::Texture_2D& depth);

		void configure_pipeline(Deferred_renderer&, graphic::Pipeline_description&);
		void configure_subpass(Deferred_renderer&, graphic::Subpass_builder&);

		void update(util::Time dt);
		void draw(vk::CommandBuffer&, graphic::Render_pass&);

	  private:
		ecs::Entity_manager&          _ecs;
		Deferred_renderer&            _renderer;
		GBuffer&                      _gbuffer;
		Directional_light_comp::Pool& _lights_directional;

		vk::UniqueDescriptorSetLayout _input_attachment_descriptor_set_layout;
		graphic::DescriptorSet        _input_attachment_descriptor_set;
	};
} // namespace mirrage::renderer
