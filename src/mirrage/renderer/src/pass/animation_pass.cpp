#include <mirrage/renderer/pass/animation_pass.hpp>

#include <mirrage/renderer/animation_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>

#include <glm/gtx/string_cast.hpp>


using mirrage::ecs::components::Transform_comp;

namespace mirrage::renderer {

	Animation_pass::Animation_pass(Deferred_renderer& renderer, ecs::Entity_manager& entities)
	  : _renderer(renderer), _ecs(entities)
	{
		_ecs.register_component_type<Pose_comp>();
		_ecs.register_component_type<Animation_comp>();
		_ecs.register_component_type<Simple_animation_controller_comp>();
	}

	void Animation_pass::update(util::Time time)
	{
		for(auto [controller, anim] : _ecs.list<Simple_animation_controller_comp, Animation_comp>()) {
			if(controller._next_animation.is_some()) {
				// fade requested
				auto& next = controller._next_animation.get_or_throw();

				if(controller._current_animation.is_nothing()
				   || controller._current_animation.get_or_throw().animation_id == next.animation_id) {
					// no fade required
					controller._current_animation = controller._next_animation;
					controller._next_animation    = util::nothing;
				} else if(controller._prev_animation.is_nothing()) {
					// no fade in progress => fade to new
					controller._prev_animation    = controller._current_animation;
					controller._current_animation = controller._next_animation;
					controller._next_animation    = util::nothing;
					controller._fade_time_left    = controller._fade_time;
				}
			}

			if(controller._fade_time_left > 0.f) {
				// fade in progress
				controller._fade_time_left = controller._fade_time_left - time.value();
				if(controller._fade_time_left <= 0.f) {
					controller._fade_time_left = 0.f;
					controller._prev_animation = util::nothing;
				}
			}

			for(auto& s : anim._animation_states) {
				if(controller._prev_animation.is_some()
				   && s.animation_id == controller._prev_animation.get_or_throw().animation_id)
					controller._prev_animation.get_or_throw().time = s.time;

				if(controller._current_animation.is_some()
				   && s.animation_id == controller._current_animation.get_or_throw().animation_id)
					controller._current_animation.get_or_throw().time = s.time;
			}

			if(controller._current_animation.is_some() && controller._prev_animation.is_some()) {
				anim._animation_states.resize(2);
				anim._animation_states[0]              = controller._prev_animation.get_or_throw();
				anim._animation_states[0].blend_weight = controller._fade_time_left / controller._fade_time;
				anim._animation_states[1]              = controller._current_animation.get_or_throw();
				anim._animation_states[1].blend_weight =
				        1.f - controller._fade_time_left / controller._fade_time;

			} else if(controller._current_animation.is_some()) {
				anim._animation_states.resize(1);
				anim._animation_states[0]              = controller._current_animation.get_or_throw();
				anim._animation_states[0].blend_weight = 1.f;
			}
		}

		for(auto& anim : _ecs.list<Animation_comp>()) {
			anim.step_time(time);
		}
	}

	void Animation_pass::draw(Frame_data& frame)
	{
		// mark all cached animations as unused
		_unused_animation_keys.clear();
		_unused_animation_keys.reserve(_animation_key_cache.size());
		for(auto&& [key, value] : _animation_key_cache) {
			(void) value;
			_unused_animation_keys.emplace(key);
		}

		// update visible animations
		for(auto& geo : frame.geometry_queue) {
			_ecs.get(geo.entity).process([&](ecs::Entity_facet& entity) {
				auto anim_mb = entity.get<Animation_comp>();
				if(anim_mb.is_nothing())
					return; // not animated

				auto& anim = anim_mb.get_or_throw();
				if(anim._animation_states.empty() || !anim._dirty)
					return; // no animation playing

				entity.get<Pose_comp>().process(
				        [&](auto& skeleton) { _update_animation(geo.entity, anim, skeleton); });
			});
		}

		// erase all unused animation keys from the cache
		for(auto&& key : _unused_animation_keys) {
			_animation_key_cache.erase(key);
		}
	}

	void Animation_pass::_update_animation(ecs::Entity_handle owner,
	                                       Animation_comp&    anim_comp,
	                                       Pose_comp&         result)
	{
		const auto& skeleton_data = *result._skeleton;
		const auto  bone_count    = skeleton_data.bone_count();

		result._bone_transforms.clear();
		result._bone_transforms.resize(std::size_t(bone_count), {{0, 0, 0, 0}, {0, 0, 0}, {0, 0, 0}});

		anim_comp._dirty = false;

		for(const auto& state : anim_comp._animation_states) {
			const auto& animation = *state.animation;

			// look up cached keys
			auto key = detail::Animation_key_cache_key{owner, state.animation_id};
			_unused_animation_keys.erase(key);
			auto cached_keys = _animation_key_cache[key];
			cached_keys.resize(std::size_t(bone_count), Animation_key{});

			// update pose
			for(auto i : util::range(std::size_t(bone_count))) {
				auto& key = cached_keys[i];

				result._bone_transforms[i] +=
				        state.blend_weight
				        * animation.bone_transform(Bone_id(i), state.time, key).get_or([&] {
					          return skeleton_data.node_transform(Bone_id(i));
				          });
			}
		}
	}


	auto Animation_pass_factory::create_pass(Deferred_renderer&   renderer,
	                                         ecs::Entity_manager& entities,
	                                         Engine&,
	                                         bool&) -> std::unique_ptr<Render_pass>
	{
		return std::make_unique<Animation_pass>(renderer, entities);
	}

	auto Animation_pass_factory::rank_device(vk::PhysicalDevice,
	                                         util::maybe<std::uint32_t>,
	                                         int current_score) -> int
	{
		return current_score;
	}

	void Animation_pass_factory::configure_device(vk::PhysicalDevice,
	                                              util::maybe<std::uint32_t>,
	                                              graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
