#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/light_comp.hpp>

#include <mirrage/graphic/render_pass.hpp>

#include <glm/gtc/quaternion.hpp>


namespace mirrage {
namespace graphic {struct Pipeline_description;}
namespace graphic {class Subpass_builder;}

namespace renderer {

	struct Shadowmap {
		graphic::Render_target_2D texture;
		graphic::Framebuffer      framebuffer;
		ecs::Entity_handle        owner;
		glm::vec3                 light_source_position;
		glm::quat                 light_source_orientation;
		ecs::Component_index      caster_count=0;

		Shadowmap(graphic::Device&, std::uint32_t size, vk::Format);
		Shadowmap(Shadowmap&&rhs)noexcept;
	};

	class Shadowmapping_pass : public Pass {
		public:
			Shadowmapping_pass(Deferred_renderer&,
			                   ecs::Entity_manager&,
			                   util::maybe<Meta_system&>);

			void update(util::Time dt)override;
			void draw(vk::CommandBuffer&, Command_buffer_source&,
			          vk::DescriptorSet global_uniform_set, std::size_t swapchain_image)override;

			auto name()const noexcept -> const char* override {return "Shadowmapping";}

		private:
			Deferred_renderer&        _renderer;
			ecs::Entity_manager&      _entities;
			vk::Format                _shadowmap_format;
			graphic::Render_target_2D _depth;

			vk::UniqueSampler _shadowmap_sampler;
			vk::UniqueSampler _shadowmap_depth_sampler;

			Directional_light_comp::Pool& _lights_directional;
			Shadowcaster_comp::Pool&      _shadowcasters;

			std::vector<Shadowmap> _shadowmaps;

			graphic::Render_pass   _render_pass;
	};

	class Shadowmapping_pass_factory : public Pass_factory {
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
