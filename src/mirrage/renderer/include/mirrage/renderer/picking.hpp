#pragma once

#include <mirrage/renderer/camera_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/entity_set_view.hpp>
#include <mirrage/utils/maybe.hpp>

#include <vector>


namespace mirrage::renderer {

	struct Pickable_radius_comp : public ecs::Component<Pickable_radius_comp> {
		static constexpr const char* name() { return "Pickable_radius"; }
		using Component::Component;

		glm::vec3 offset{0, 0, 0};
		float     radius{1.f};
	};
	sf2_structDef(Pickable_radius_comp, offset, radius);


	struct Pick_result {
		ecs::Entity_handle entity;
		float              distance = std::numeric_limits<float>::infinity();

		constexpr friend bool operator<(const Pick_result& lhs, const Pick_result& rhs)
		{
			return lhs.distance < rhs.distance;
		}
	};

	class Picking {
	  public:
		Picking(util::maybe<ecs::Entity_manager&>, util::maybe<Camera_state>& active_camera);

		template <typename... TagComponents>
		std::vector<Pick_result> pick(glm::vec2                        screen_position,
		                              util::maybe<const Camera_state&> camera = {}) const;

		template <typename... TagComponents>
		util::maybe<Pick_result> pick_closest(glm::vec2                        screen_position,
		                                      int                              nth    = 0,
		                                      bool                             loop   = true,
		                                      util::maybe<const Camera_state&> camera = {}) const;

	  private:
		util::maybe<ecs::Entity_manager&> _ecs;
		util::maybe<Camera_state>&        _active_camera;

		template <typename... TagComponents, typename F>
		void ray_march(glm::vec2 screen_position, util::maybe<const Camera_state&> camera, F&& on_hit) const;
	};



	// implementation
	namespace detail {
		struct Ray {
			glm::vec3 origin;
			glm::vec3 dir;

			Ray(glm::vec2 screen_position, const Camera_state& camera) : origin(camera.eye_position)
			{
				const auto screen_pos = glm::unProject(glm::vec3{screen_position, 1.f},
				                                       glm::mat4(1.f),
				                                       camera.pure_projection,
				                                       camera.viewport);

				const auto world_pos = glm::vec3(camera.inv_view * glm::vec4{screen_pos, 1.f});

				dir = glm::normalize(world_pos - camera.eye_position);
			}

			/// return distance to origin on hit or -1 on miss
			float intersects(glm::vec3 sphere_center, float sphere_radius) const
			{
				const auto p       = origin - sphere_center;
				const auto linear  = glm::dot(dir, p);
				const auto discr_b = glm::length2(p) - sphere_radius * sphere_radius;
				if(discr_b > 0.f && linear > 0.f)
					return -1; // sphere is behind us

				const auto discriminant = linear * linear - discr_b;
				if(discriminant < 0.f)
					return -1;
				else if(discriminant < 0.1f * 0.1f)
					return linear; // linear
				else {
					const auto discr_sqrt = glm::sqrt(discriminant);
					const auto dist       = -linear - discr_sqrt;
					if(dist > 0)
						return dist;
					else
						return -linear + discr_sqrt;
				}
			}
		};

	} // namespace detail

	template <typename... TagComponents, typename F>
	void Picking::ray_march(glm::vec2                        screen_position,
	                        util::maybe<const Camera_state&> camera,
	                        F&&                              on_hit) const
	{
		using ecs::components::Transform_comp;

		if(_ecs.is_nothing())
			return;

		if(camera.is_nothing()) {
			if(_active_camera.is_some()) {
				camera = _active_camera.get_or_throw();
			} else {
				return;
			}
		}

		const auto ray = detail::Ray(screen_position, camera.get_or_throw());

		for(auto&& e : _ecs.get_or_throw().list<ecs::Entity_facet, Transform_comp, TagComponents...>()) {
			auto&& entity    = std::get<0>(e);
			auto&& transform = std::get<1>(e);

			auto  offset = glm::vec3(0, 0, 0);
			float radius = 0.f;
			entity.process([&](const Pickable_radius_comp& c) {
				offset = c.offset;
				radius = c.radius;
			});

			if(radius == 0) {
				entity.process([&](const Model_comp& c) {
					offset = c.bounding_sphere_offset();
					radius = c.bounding_sphere_radius();
				});
			}

			if constexpr(sizeof...(TagComponents) == 0) {
				if(radius == 0)
					radius = 1.f;
			}

			if(radius > 0) {
				const auto scale = util::max(transform.scale.x, transform.scale.y, transform.scale.z);
				if(auto dist = ray.intersects(offset + transform.position, radius * scale); dist >= 0.f) {
					on_hit(Pick_result{entity, dist});
				}
			}
		}
	}

	template <typename... TagComponents>
	std::vector<Pick_result> Picking::pick(glm::vec2                        screen_position,
	                                       util::maybe<const Camera_state&> camera) const
	{
		auto result = std::vector<Pick_result>();
		ray_march<TagComponents...>(
		        screen_position, camera, [&](auto hit) { result.emplace_back(std::move(hit)); });

		std::sort(result.begin(), result.end());

		return result;
	}

	template <typename... TagComponents>
	util::maybe<Pick_result> Picking::pick_closest(glm::vec2                        screen_position,
	                                               int                              nth,
	                                               bool                             loop,
	                                               util::maybe<const Camera_state&> camera) const
	{
		constexpr auto tmp_buffer_size = 16;

		if(nth >= tmp_buffer_size) {
			auto tmp_buffer = pick<TagComponents...>(screen_position, camera);
			if(tmp_buffer.size() <= std::size_t(nth))
				return util::nothing;

			std::nth_element(tmp_buffer.begin(), tmp_buffer.begin() + nth, tmp_buffer.end());
			return tmp_buffer[nth];

		} else {
			auto tmp_buffer  = std::array<Pick_result, tmp_buffer_size>();
			auto next_insert = tmp_buffer.begin();
			auto end         = tmp_buffer.begin() + nth + 1;

			ray_march<TagComponents...>(screen_position, camera, [&](auto hit) {
				auto position = std::upper_bound(tmp_buffer.begin(), end, hit);
				if(position != end) {
					if(position != next_insert) {
						std::move(position, next_insert - 1, position + 1);
					}

					*position   = std::move(hit);
					next_insert = std::min(next_insert + 1, end);
				}
			});

			if(next_insert == end) {
				return util::just(std::move(tmp_buffer[nth]));

			} else if(next_insert != tmp_buffer.begin()) {
				return util::just(std::move(tmp_buffer[0]));

			} else {
				return util::nothing;
			}
		}
	}

} // namespace mirrage::renderer
