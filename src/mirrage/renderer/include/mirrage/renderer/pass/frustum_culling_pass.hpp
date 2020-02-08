#pragma once

#include <mirrage/renderer/billboard.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>
#include <mirrage/renderer/object_router.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/entity_set_view.hpp>


namespace mirrage::renderer {

	class Frustum_culling_pass_factory;

	class Frustum_culling_pass : public Render_pass {
	  public:
		using Factory = Frustum_culling_pass_factory;

		Frustum_culling_pass(Deferred_renderer&, ecs::Entity_manager&);

		template <typename... Passes>
		void pre_draw(Frame_data&, Object_router<Passes...>&);

		template <typename... Passes>
		void on_draw(Frame_data&, Object_router<Passes...>&);

		auto name() const noexcept -> const char* override { return "Frustum_culling"; }

	  private:
		ecs::Entity_manager& _ecs;
	};

	class Frustum_culling_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Frustum_culling_pass_factory>();
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
	void Frustum_culling_pass::pre_draw(Frame_data& fd, Object_router<Passes...>& router)
	{
		fd.camera_culling_mask = router.add_viewer(fd.camera->view_projection, true);
	}

	template <typename... Passes>
	void Frustum_culling_pass::on_draw(Frame_data& fd, Object_router<Passes...>& router)
	{
		using mirrage::ecs::components::Transform_comp;

		// directional lights
		for(auto& [entity, light, transform] :
		    _ecs.list<ecs::Entity_facet, Directional_light_comp, Transform_comp>()) {
			if(light.color().length() * light.intensity() > 0.000001f) {
				router.process_always_visible_obj(~Culling_mask(0), entity, transform, light);
			}
		}

		// point lights
		for(auto& [entity, light, transform] :
		    _ecs.list<ecs::Entity_facet, Point_light_comp, Transform_comp>()) {
			if(light.color().length() * light.intensity() > 0.000001f) {
				router.process_obj(transform.position, light.calc_radius(), true, entity, transform, light);
			}
		}


		// decals
		for(auto& [entity, decal_comp, transform] :
		    _ecs.list<ecs::Entity_facet, Decal_comp, Transform_comp>()) {
			for(auto&& dc : decal_comp.decals) {
				auto pos   = transform.position + dc.offset;
				auto range = glm::length(glm::vec3(dc.size, dc.thickness));

				if(dc.active && dc.material.ready()) {
					router.process_obj(pos, range, true, dc, transform);
				}
			}
		}


		// particle systems
		if(_renderer.settings().particles) {
			for(auto& particle_sys : _ecs.list<Particle_system_comp>()) {
				if(!particle_sys.particle_system.cfg().ready())
					continue;

				for(auto&& emitter : particle_sys.particle_system.emitters()) {
					if(emitter.active()) {
						auto pos          = particle_sys.particle_system.emitter_position(emitter);
						auto update_range = emitter.cfg().type->update_range;
						auto draw_range   = emitter.cfg().type->draw_range;
						auto cam_only     = !emitter.cfg().type->shadowcaster;

						router.process_obj(pos,
						                   update_range,
						                   cam_only,
						                   particle_sys,
						                   emitter,
						                   Particle_system_update_tag{});
						router.process_obj(pos, draw_range, cam_only, particle_sys, emitter);
					}
				}
			}
		}


		// models
		for(auto& [entity, model, transform] : _ecs.list<ecs::Entity_facet, Model_comp, Transform_comp>()) {
			auto              entity_pos = transform.position;
			auto              dir_mat    = transform.to_mat3();
			auto              scale      = util::max(transform.scale.x, transform.scale.y, transform.scale.z);
			ecs::Entity_facet e          = entity;
			const auto        cam_only   = !e.has<Shadowcaster_comp>();

			auto& sub_meshes = model.model()->sub_meshes();
			auto  offset     = model.model()->bounding_sphere_offset();
			auto  radius     = model.model()->bounding_sphere_radius();

			const auto sphere_center = entity_pos + dir_mat * offset;
			const auto sphere_radius = radius * scale;

			auto& model_     = model;
			auto& transform_ = transform;

			router.process_sub_objs(sphere_center, sphere_radius, cam_only, [&](auto mask, auto&& draw) {
				if(!router.process_always_visible_obj(mask, e, transform_, model_)) {
					if(sub_meshes.size() <= 4) {
						// skip sub-object culling if the number of sub-objects is low, to reduce culling overhead
						for(auto& sub : sub_meshes) {
							router.process_always_visible_obj(mask, e, transform_, model_, sub);
						}

					} else {
						for(auto& sub : sub_meshes) {
							auto sub_offset = sub.bounding_sphere_offset;
							auto sub_radius = sub.bounding_sphere_radius;

							const auto sub_sphere_center = entity_pos + dir_mat * sub_offset;
							const auto sub_sphere_radius = sub_radius * scale;

							draw(sub_sphere_center, sub_sphere_radius, e, transform_, model_, sub);
						}
					}
				}
			});
		}
	}

} // namespace mirrage::renderer
