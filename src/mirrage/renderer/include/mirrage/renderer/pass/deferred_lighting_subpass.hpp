#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>


namespace mirrage::graphic {
	struct Pipeline_description;
	class Subpass_builder;
} // namespace mirrage::graphic

namespace mirrage::renderer {

	struct Deferred_push_constants {
		glm::mat4 model{0};
		glm::vec4 light_color{0, 0, 0, 0}; //< for light-subpass; A=intensity
		glm::vec4 light_data{0, 0, 0, 0};  //< for light-subpass; R=src_radius, GBA=direction
		glm::vec4 light_data2{0, 0, 0, 0}; //< for light-subpass; R=shadowmapID
		glm::vec4 shadow_color{0, 0, 0, 0};
	};
	static_assert(sizeof(Deferred_push_constants) <= 4096, "Too large for push constants!");


	class Deferred_lighting_subpass {
	  public:
		Deferred_lighting_subpass(Deferred_renderer&, ecs::Entity_manager&, graphic::Texture_2D& depth);

		void configure_pipeline(Deferred_renderer&, graphic::Pipeline_description&);
		void configure_subpass(Deferred_renderer&, graphic::Subpass_builder&);

		void on_render_pass_configured(graphic::Render_pass&, graphic::Framebuffer&) {}

		void update(util::Time dt);
		void draw(Frame_data&, graphic::Render_pass&);

		void handle_obj(Frame_data&,
		                Culling_mask,
		                const glm::quat& orientation,
		                const glm::vec3& position,
		                const Directional_light_comp&);
		void handle_obj(Frame_data&, Culling_mask, const glm::vec3& position, const Point_light_comp&);


	  private:
		ecs::Entity_manager& _ecs;
		Deferred_renderer&   _renderer;
		GBuffer&             _gbuffer;

		graphic::Mesh                 _point_light_mesh;
		vk::UniqueDescriptorSetLayout _input_attachment_descriptor_set_layout;
		graphic::DescriptorSet        _input_attachment_descriptor_set;

		std::vector<Deferred_push_constants> _directional_lights;
		std::vector<Deferred_push_constants> _point_lights;
	};
} // namespace mirrage::renderer
