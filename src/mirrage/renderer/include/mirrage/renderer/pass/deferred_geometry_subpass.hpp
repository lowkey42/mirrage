#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/model_comp.hpp>


namespace mirrage::graphic {
	struct Pipeline_description;
	class Subpass_builder;
} // namespace mirrage::graphic

namespace mirrage::renderer {

	class Deferred_geometry_subpass {
	  public:
		Deferred_geometry_subpass(Deferred_renderer&, ecs::Entity_manager&);

		void configure_pipeline(Deferred_renderer&, graphic::Pipeline_description&);
		void configure_subpass(Deferred_renderer&, graphic::Subpass_builder&);

		void update(util::Time dt);
		void pre_draw(vk::CommandBuffer&);
		void draw(vk::CommandBuffer&, graphic::Render_pass&);

	  private:
		Deferred_renderer& _renderer;
		Model_comp::Pool&  _models;
	};
} // namespace mirrage::renderer
