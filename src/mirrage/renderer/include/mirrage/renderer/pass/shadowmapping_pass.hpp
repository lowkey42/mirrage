#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>

#include <glm/gtc/quaternion.hpp>


namespace mirrage::graphic {
	struct Pipeline_description;
	class Subpass_builder;
} // namespace mirrage::graphic

namespace mirrage::renderer {

	struct Shadowmap {
		graphic::Render_target_2D texture;
		graphic::Framebuffer      framebuffer;
		ecs::Entity_handle        owner;
		glm::vec3                 light_source_position;
		glm::quat                 light_source_orientation;
		ecs::Component_index      caster_count = 0;

		Shadowmap(graphic::Device&, std::int32_t size, vk::Format);
		Shadowmap(Shadowmap&& rhs) noexcept;
	};

	class Shadowmapping_pass_factory;

	class Shadowmapping_pass : public Render_pass {
	  public:
		using Factory = Shadowmapping_pass_factory;

		Shadowmapping_pass(Deferred_renderer&, ecs::Entity_manager&);

		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Shadowmapping"; }

	  private:
		Deferred_renderer&        _renderer;
		ecs::Entity_manager&      _entities;
		vk::Format                _shadowmap_format;
		graphic::Render_target_2D _depth;

		vk::UniqueSampler _shadowmap_sampler;
		vk::UniqueSampler _shadowmap_depth_sampler;

		std::vector<Shadowmap> _shadowmaps;

		graphic::Render_pass _render_pass;
	};

	class Shadowmapping_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Shadowmapping_pass_factory>();
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
