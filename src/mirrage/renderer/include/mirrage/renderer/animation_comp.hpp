#pragma once

#include <mirrage/renderer/animation.hpp>

#include <mirrage/ecs/component.hpp>

#include <glm/mat4x4.hpp>

#include <vector>


namespace mirrage::renderer {

	class Skeleton_comp : public ecs::Component<Skeleton_comp> {
	  public:
		static constexpr const char* name() { return "Skeleton"; }

		Skeleton_comp() = default;
		Skeleton_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		std::vector<glm::mat4> bone_transforms;
	};

	class Animation_comp : public ecs::Component<Animation_comp> {
	  public:
		static constexpr const char* name() { return "Animation"; }
		friend void                  load_component(ecs::Deserializer& state, Animation_comp&);
		friend void                  save_component(ecs::Serializer& state, const Animation_comp&);

		Animation_comp() = default;
		Animation_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		void animation(util::Str_id); ///< play preloaded; for small or frequently used animations
		void animation(asset::AID);   ///< load + play; for large one-time animations

		// TODO: pause, stop, speed, ...

	  private:
		friend class Animation_pass;

		asset::AID           _skeleton_id;
		asset::Ptr<Skeleton> _skeleton;

		std::unordered_map<util::Str_id, asset::Ptr<Animation>> _preloaded_animations;
		asset::Ptr<Animation>                                   _current_animation;

		std::vector<Animation_key> _animation_keys;

		float _time = 0.f;
	};

} // namespace mirrage::renderer
