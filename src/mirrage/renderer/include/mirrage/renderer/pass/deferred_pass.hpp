#pragma once

#include <mirrage/renderer/pass/deferred_geometry_subpass.hpp>
#include <mirrage/renderer/pass/deferred_lighting_subpass.hpp>

#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/graphic/render_pass.hpp>


namespace mirrage {
namespace renderer {

	struct Deferred_push_constants {
		glm::mat4 model;
		glm::vec4 light_color; //< for light-subpass; A=intensity
		glm::vec4 light_data;  //< for light-subpass; R=src_radius, GBA=direction
		glm::vec4 light_data2; //< for light-subpass; R=shadowmapID
	};
	static_assert(sizeof(Deferred_push_constants)<=4096, "Too large for push constants!");

	// populates linear-depth, albedo/matId, matData, writes the direct lighting results
	// to the target color-Buffer and just the diffuse lighting to the other color-Buffer
	class Deferred_pass : public Pass {
		public:
			Deferred_pass(Deferred_renderer&,
			              ecs::Entity_manager&,
			              util::maybe<Meta_system&>,
			              graphic::Render_target_2D& color_target,
			              graphic::Render_target_2D& color_target_diff);


			void update(util::Time dt) override;
			void draw(vk::CommandBuffer&, Command_buffer_source&,
			          vk::DescriptorSet global_uniform_set, std::size_t swapchain_image) override;

			void shrink_to_fit() override;

			auto name()const noexcept -> const char* override {return "Deferred";}

		private:
			graphic::Render_target_2D _depth;

			Deferred_geometry_subpass _gpass;
			Deferred_lighting_subpass _lpass;

			graphic::Framebuffer _gbuffer_framebuffer;
			graphic::Render_pass _render_pass;
	};

	class Deferred_pass_factory : public Pass_factory {
		public:
			auto create_pass(Deferred_renderer&,
			                 ecs::Entity_manager&,
			                 util::maybe<Meta_system&>,
			                 bool& write_first_pp_buffer) -> std::unique_ptr<Pass> override;

			auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
			                 int current_score) -> int override;

			void configure_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
			                      graphic::Device_create_info&) override;
	};

}
}
