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
	}

	void Animation_pass::update(util::Time time)
	{
		for(auto& anim : _ecs.list<Animation_comp>()) {
			// TODO: transitions and stuff
			anim.step_time(time);
		}
	}

	void Animation_pass::draw(Frame_data& frame)
	{
		// FIXME: currently uploads a new skeleton for each sub-mesh!
		for(auto& geo : frame.geometry_queue) {
			_ecs.get(geo.entity).process([&](ecs::Entity_facet& entity) {
				auto anim_mb = entity.get<Animation_comp>();
				if(anim_mb.is_nothing())
					return; // not animated

				auto& anim = anim_mb.get_or_throw();
				if(!anim._skeleton || !anim._current_animation || !anim._dirty)
					return; // no animation playing

				entity.get<Pose_comp>().process([&](auto& skeleton) { _update_animation(anim, skeleton); });
			});
		}
	}

	void Animation_pass::_update_animation(Animation_comp& anim_comp, Pose_comp& result)
	{
		const auto& skeleton_data = *anim_comp._skeleton;
		const auto  bone_count    = skeleton_data.bone_count();
		const auto& animation     = *anim_comp._current_animation;

		result.bone_transforms.reserve(std::size_t(bone_count));
		result.bone_transforms.clear();

		anim_comp._dirty = false;
		anim_comp._animation_keys.resize(std::size_t(bone_count), Animation_key{});

		for(auto i : util::range(bone_count)) {
			auto parent = skeleton_data.parent_bone(i).process(
			        glm::mat4(1), [&](auto idx) { return result.bone_transforms[std::size_t(idx)]; });

			auto& key = anim_comp._animation_keys[std::size_t(i)];

			auto local = animation.bone_transform(i, anim_comp._time, key).get_or([&] {
				return skeleton_data.node_transform(i);
			});

			result.bone_transforms.emplace_back(parent * local);
		}

		auto inv_root = glm::inverse(skeleton_data.node_transform(0));
		for(auto i : util::range(bone_count)) {
			result.bone_transforms[std::size_t(i)] =
			        inv_root * result.bone_transforms[std::size_t(i)] * skeleton_data.inv_bind_pose(i);
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
