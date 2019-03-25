#include <mirrage/renderer/pass/frustum_culling_pass.hpp>

#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>

#include <glm/gtc/matrix_access.hpp>


using mirrage::ecs::components::Transform_comp;

namespace mirrage::renderer {

	namespace {
		using Frustum_planes = std::array<glm::vec4, 6>;
		struct Culling_viewer {
			Frustum_planes planes;
			bool           shadowmap;

			Culling_viewer() = default;
			Culling_viewer(Frustum_planes planes, bool shadowmap = false)
			  : planes(planes), shadowmap(shadowmap)
			{
			}
		};

		auto is_visible(const Culling_viewer& viewer, glm::vec3 position, float radius)
		{
			const auto p = glm::vec4(position, 1.f);

			auto result = true;
			for(auto& plane : viewer.planes)
				result = result & (glm::dot(plane, p) > -radius);

			return result;
		}

		auto norm_plane(glm::vec4 p) { return p / glm::length(glm::vec3(p.x, p.y, p.z)); }
		auto extract_planes(const glm::mat4& cam_view_proj) -> Frustum_planes
		{
			return {
			        norm_plane(row(cam_view_proj, 3) + row(cam_view_proj, 0)), // left
			        norm_plane(row(cam_view_proj, 3) - row(cam_view_proj, 0)), // right
			        norm_plane(row(cam_view_proj, 3) - row(cam_view_proj, 1)), // top
			        norm_plane(row(cam_view_proj, 3) + row(cam_view_proj, 1)), // bottom
			        norm_plane(row(cam_view_proj, 3) + row(cam_view_proj, 2)), // near
			        norm_plane(row(cam_view_proj, 3) - row(cam_view_proj, 2))  // far
			};
		}

	} // namespace

	Frustum_culling_pass::Frustum_culling_pass(Deferred_renderer& renderer, ecs::Entity_manager& entities)
	  : _renderer(renderer), _ecs(entities)
	{
	}

	void Frustum_culling_pass::update(util::Time) {}

	void Frustum_culling_pass::draw(Frame_data& frame)
	{
		if(_renderer.active_camera().is_nothing())
			return;

		auto viewers = std::vector<Culling_viewer>();
		viewers.emplace_back(extract_planes(_renderer.active_camera().get_or_throw().view_projection));

		for(auto& [entity, light, transform] :
		    _ecs.list<ecs::Entity_handle, Directional_light_comp, Transform_comp>()) {
			// directional lights are always treated as visible (for now).
			// so just check if its "on"
			if(light.color().length() * light.intensity() > 0.000001f) {
				auto viewer_mask = _renderer.gbuffer().shadowmapping_enabled && light.shadowcaster()
				                           ? (std::uint32_t(1) << viewers.size())
				                           : 0;
				frame.light_queue.emplace_back(entity, transform, light, viewer_mask);

				if(_renderer.gbuffer().shadowmapping_enabled && light.shadowcaster()
				   && light.needs_update()) {
					viewers.emplace_back(extract_planes(light.calc_shadowmap_view_proj(transform)), true);
				}
			}
		}

		for(auto& [entity, light, transform] :
		    _ecs.list<ecs::Entity_handle, Point_light_comp, Transform_comp>()) {

			if(light.color().length() * light.intensity() > 0.000001f
			   && is_visible(viewers.front(), transform.position, light.calc_radius())) {
				auto viewer_mask = 0;
				frame.light_queue.emplace_back(entity, transform, light, viewer_mask);
			}
		}

		for(auto& [entity, billboard_comp, transform] :
		    _ecs.list<ecs::Entity_handle, Billboard_comp, Transform_comp>()) {

			for(auto&& bb : billboard_comp.billboards) {
				if(bb.absolute_screen_space) {
					frame.billboard_queue.emplace_back(bb);
				} else {
					auto position = transform.position + bb.offset;
					if(bb.active && bb.material.ready()
					   && is_visible(viewers.front(), position, glm::length(bb.size))) {
						frame.billboard_queue.emplace_back(bb);
						frame.billboard_queue.back().offset = position;
					}
				}
			}
		}

		for(auto& [entity, decal_comp, transform] :
		    _ecs.list<ecs::Entity_handle, Decal_comp, Transform_comp>()) {

			for(auto&& dc : decal_comp.decals) {
				auto position = transform.position + dc.offset;
				auto size     = glm::length(glm::vec3(dc.size, dc.thickness));

				if(dc.active && dc.material.ready() && is_visible(viewers.front(), position, size)) {
					frame.decal_queue.emplace_back(dc, transform.to_mat4());
				}
			}
		}

		if(_renderer.settings().particles) {
			for(auto& particle_sys : _ecs.list<Particle_system_comp>()) {
				if(!particle_sys.particle_system.cfg().ready())
					continue;

				for(auto&& emitter : particle_sys.particle_system.emitters()) {
					if(emitter.active()) {
						auto pos          = particle_sys.particle_system.emitter_position(emitter);
						auto update_range = emitter.cfg().type->update_range;
						auto draw_range   = emitter.cfg().type->draw_range;
						auto shadowcaster = emitter.cfg().type->shadowcaster;

						auto draw_mask = draw_range < 0.f ? ~std::uint32_t(0) : std::uint32_t(0);
						auto update    = update_range < 0.f;

						if(!draw_mask) {
							for(auto i = 0u; i < viewers.size(); i++) {
								if(viewers[i].shadowmap && !shadowcaster)
									continue;

								if(is_visible(viewers[i], pos, draw_range)) {
									draw_mask |= std::uint32_t(1) << i;
								} else if(!update && is_visible(viewers[i], pos, update_range)) {
									update = true;
								}
							}
						}

						if(update || draw_mask) {
							frame.particle_queue.emplace_back(particle_sys.owner_handle(),
							                                  emitter,
							                                  particle_sys.particle_system,
							                                  particle_sys.particle_system.effectors(),
							                                  draw_mask);
						}
					}
				}
			}
		}

		for(auto& [entity, model, transform] : _ecs.list<ecs::Entity_facet, Model_comp, Transform_comp>()) {
			auto entity_pos = transform.position;
			auto dir_mat    = transform.to_mat3();
			auto scale      = util::max(transform.scale.x, transform.scale.y, transform.scale.z);

			auto& sub_meshes = model.model()->sub_meshes();
			auto  offset     = model.model()->bounding_sphere_offset();
			auto  radius     = model.model()->bounding_sphere_radius();

			const auto sphere_center = entity_pos + dir_mat * offset;
			const auto sphere_radius = radius * scale;

			auto main_mask = std::uint32_t(0);
			for(auto i = 0u; i < viewers.size(); i++) {
				if(viewers[i].shadowmap && !entity.has<Shadowcaster_comp>())
					continue;

				if(is_visible(viewers[i], sphere_center, sphere_radius)) {
					main_mask |= std::uint32_t(1) << i;
				}
			}

			if(main_mask != 0) {
				for(auto sub_idx = 0u; sub_idx < sub_meshes.size(); sub_idx++) {
					auto& sub = sub_meshes[sub_idx];

					auto sub_offset = sub.bounding_sphere_offset;
					auto sub_radius = sub.bounding_sphere_radius;

					const auto sub_sphere_center = entity_pos + dir_mat * sub_offset;
					const auto sub_sphere_radius = sub_radius * scale;

					auto mask = std::uint32_t(0);
					if(!model.model()->rigged()) {
						for(auto i = 0u; i < viewers.size(); i++) {
							auto bit = std::uint32_t(1) << i;

							if((main_mask & bit) != 0
							   && is_visible(viewers[i], sub_sphere_center, sub_sphere_radius)) {
								mask |= bit;
							}
						}

					} else {
						mask = main_mask;
					}

					if(mask != 0) {
						frame.geometry_queue.emplace_back(entity.handle(),
						                                  transform.position,
						                                  transform.orientation,
						                                  transform.scale,
						                                  &*model.model(),
						                                  sub.material->substance_id(),
						                                  sub_idx,
						                                  mask);
					}
				}
			}
		}
	}


	auto Frustum_culling_pass_factory::create_pass(Deferred_renderer&                renderer,
	                                               util::maybe<ecs::Entity_manager&> entities,
	                                               Engine&,
	                                               bool&) -> std::unique_ptr<Render_pass>
	{
		return std::make_unique<Frustum_culling_pass>(
		        renderer, entities.get_or_throw("Frustum_culling_pass requires an entitymanager."));
	}

	auto Frustum_culling_pass_factory::rank_device(vk::PhysicalDevice,
	                                               util::maybe<std::uint32_t>,
	                                               int current_score) -> int
	{
		return current_score;
	}

	void Frustum_culling_pass_factory::configure_device(vk::PhysicalDevice,
	                                                    util::maybe<std::uint32_t>,
	                                                    graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
