#pragma once

#include <mirrage/renderer/animation.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>
#include <mirrage/renderer/object_router.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/graphic/render_pass.hpp>

#include <glm/gtc/quaternion.hpp>


namespace mirrage::graphic {
	struct Pipeline_description;
	class Subpass_builder;
} // namespace mirrage::graphic

namespace mirrage::renderer {

	class Model_comp;

	struct Shadowmap {
		graphic::Render_target_2D texture;
		graphic::Framebuffer      framebuffer;
		ecs::Entity_handle        owner;
		glm::vec3                 light_source_position;
		glm::quat                 light_source_orientation;

		util::maybe<Culling_mask>   culling_mask;
		graphic::Command_pool_group model_group;
		graphic::Command_pool_group model_animated_group;
		graphic::Command_pool_group model_animated_dqs_group;
		glm::mat4                   view_proj;

		Shadowmap(Deferred_renderer& r, graphic::Device&, std::int32_t size, vk::Format);
		Shadowmap(Shadowmap&& rhs) noexcept;
	};

	class Shadowmapping_pass_factory;

	class Shadowmapping_pass : public Render_pass {
	  public:
		using Factory = Shadowmapping_pass_factory;

		Shadowmapping_pass(Deferred_renderer&, ecs::Entity_manager&);

		void pre_draw(Frame_data&);

		template <typename... Passes>
		void handle_obj(Frame_data&,
		                Object_router<Passes...>& router,
		                Culling_mask,
		                const glm::quat& orientation,
		                const glm::vec3& position,
		                Directional_light_comp&);

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
		                Skinning_type skinning_type,
		                std::uint32_t pose_offset);

		void post_draw(Frame_data&);

		auto name() const noexcept -> const char* override { return "Shadowmapping"; }

	  private:
		struct Shadowmapping_pass_impl_helper;

		ecs::Entity_manager&      _entities;
		vk::Format                _shadowmap_format;
		graphic::Render_target_2D _depth;
		bool                      _first_frame = true;

		vk::UniqueSampler _shadowmap_sampler;
		vk::UniqueSampler _shadowmap_depth_sampler;

		std::vector<Shadowmap> _shadowmaps;

		graphic::Render_pass                 _render_pass;
		const graphic::Render_pass_stage_ref _model_stage;
		const graphic::Render_pass_stage_ref _model_animated_stage;
		const graphic::Render_pass_stage_ref _model_animate_dqs_stage;
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


	template <typename... Passes>
	void Shadowmapping_pass::handle_obj(Frame_data&,
	                                    Object_router<Passes...>& router,
	                                    Culling_mask,
	                                    const glm::quat&        orientation,
	                                    const glm::vec3&        position,
	                                    Directional_light_comp& light)
	{
		if(_renderer.gbuffer().shadowmapping_enabled && light.color().length() * light.intensity() > 0.0001f
		   && light.shadowcaster() && light.needs_update()) {

			if(light.shadowmap_id() == -1) {
				auto next_free_shadowmap = std::find_if(
				        _shadowmaps.begin(), _shadowmaps.end(), [&](auto& s) { return !s.owner; });

				if(next_free_shadowmap != _shadowmaps.end()) {
					light.shadowmap_id(
					        static_cast<int>(std::distance(_shadowmaps.begin(), next_free_shadowmap)));
					next_free_shadowmap->owner = light.owner_handle();

				} else {
					return;
				}
			}

			light.on_update();
			auto& shadowmap                    = _shadowmaps[light.shadowmap_id()];
			shadowmap.light_source_position    = position;
			shadowmap.light_source_orientation = orientation;
			shadowmap.view_proj                = light.calc_shadowmap_view_proj(position, orientation);
			shadowmap.culling_mask             = router.add_viewer(shadowmap.view_proj, false);

			if(!shadowmap.model_group)
				shadowmap.model_group = _renderer.reserve_secondary_command_buffer_group();

			if(!shadowmap.model_animated_group)
				shadowmap.model_animated_group = _renderer.reserve_secondary_command_buffer_group();
		}
	}

} // namespace mirrage::renderer
