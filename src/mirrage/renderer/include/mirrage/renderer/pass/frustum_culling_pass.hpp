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
		ecs::Entity_manager&           _ecs;
		std::vector<async::task<void>> _tasks;
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
		for(auto& [light, transform] : _ecs.list<Directional_light_comp, Transform_comp>()) {
			if(light.color().length() * light.intensity() > 0.000001f) {
				router.process_always_visible_obj(
				        ~Culling_mask(0), transform.orientation, transform.position, light);
			}
		}

		// point lights
		for(auto& [light, transform] : _ecs.list<Point_light_comp, Transform_comp>()) {
			if(light.color().length() * light.intensity() > 0.000001f) {
				router.process_obj(transform.position, light.calc_radius(), true, transform.position, light);
			}
		}


		// decals
		for(auto& [entity, decal_comp, transform] :
		    _ecs.list<ecs::Entity_facet, Decal_comp, Transform_comp>()) {
			for(auto&& dc : decal_comp.decals) {
				auto pos   = transform.position + dc.offset;
				auto range = glm::length(glm::vec3(dc.size, dc.thickness));

				if(dc.active && dc.material.ready()) {
					router.process_obj(pos, range, true, dc, transform.to_mat4());
				}
			}
		}


		// particle systems
		if(_renderer.settings().particles) {
			for(auto& [owner, particle_sys] : _ecs.list<ecs::Entity_facet, Particle_system_comp>()) {
				if(!particle_sys.particle_system.cfg().ready())
					continue;

				auto emissive = owner.template get<Material_property_comp>().process(
				        glm::vec4(1, 1, 1, 1000), [](auto& m) { return m.emissive_color; });

				for(auto&& emitter : particle_sys.particle_system.emitters()) {
					if(emitter.active()) {
						auto& sys          = particle_sys.particle_system;
						auto  pos          = sys.emitter_position(emitter);
						auto& type         = *emitter.cfg().type;
						auto  update_range = type.update_range;
						auto  draw_range   = type.draw_range;
						auto  cam_only     = !type.shadowcaster;

						router.process_obj(
						        pos, update_range, cam_only, sys, emitter, Particle_system_update_tag{});
						router.process_obj(pos, draw_range, cam_only, emissive, sys, emitter);
					}
				}
			}
		}


		// models
		auto model_handler = [&](auto&& element) {
			auto& [entity, model_comp, transform] = element;

			ecs::Entity_facet entity_     = entity;
			auto&             transform_  = transform;
			auto&             model_comp_ = model_comp;
			const auto        cam_only    = !entity_.has<Shadowcaster_comp>();

			const auto offset = model_comp.bounding_sphere_offset();
			const auto radius = model_comp.bounding_sphere_radius();

			const auto mat = transform_.to_mat4() * model_comp_.local_transform();

			const auto sphere_center = glm::vec3(mat * glm::vec4(offset, 1.f));
			const auto sphere_radius = glm::length(glm::vec3(mat * glm::vec4(radius, 0.f, 0.f, 0.f)));

			router.process_sub_objs(sphere_center, sphere_radius, cam_only, [&](auto mask, auto&& draw) {
				auto&       model      = *model_comp_.model();
				const auto& sub_meshes = model.sub_meshes();

				auto emissive = entity_.get<Material_property_comp>().process(
				        glm::vec4(1, 1, 1, 1000), [](auto& m) { return m.emissive_color; });

				auto material_overrides = entity_.get<Material_override_comp>().process(
				        gsl::span<Material_override>(),
				        [&](auto& mo) { return gsl::span<Material_override>(mo.material_overrides); });

				if(!router.process_always_visible_obj(
				           mask, entity_, emissive, mat, model, material_overrides)) {
					if(sub_meshes.size() <= 4) {
						// skip sub-object culling if the number of sub-objects is low, to reduce culling overhead
						for(auto&& [index, sub] : util::with_index(sub_meshes)) {
							auto material = material_overrides.size() > index ? material_overrides[index]
							                                                  : Material_override{};
							router.process_always_visible_obj(
							        mask, entity_, emissive, mat, model, material, sub);
						}

					} else {
						for(auto&& [index, sub] : util::with_index(sub_meshes)) {
							auto sub_offset = sub.bounding_sphere_offset;
							auto sub_radius = sub.bounding_sphere_radius;

							const auto sub_sphere_center = glm::vec3(mat * glm::vec4(sub_offset, 1.f));
							const auto sub_sphere_radius =
							        glm::length(glm::vec3(mat * glm::vec4(sub_radius, 0.f, 0.f, 0.f)));

							auto material = material_overrides.size() > index ? material_overrides[index]
							                                                  : Material_override{};
							draw(sub_sphere_center,
							     sub_sphere_radius,
							     entity_,
							     emissive,
							     mat,
							     model,
							     material,
							     sub);
						}
					}
				}
			});
		};

		auto model_view = _ecs.list<ecs::Entity_facet, Model_comp, Transform_comp>();
		async::parallel_for(_renderer.scheduler(),
		                    ecs::entity_set_partitioner(model_view, _renderer.scheduler_threads().size()),
		                    model_handler);
	}

} // namespace mirrage::renderer
