#pragma once

#include <mirrage/renderer/animation.hpp>

#include <mirrage/ecs/component.hpp>

#include <glm/mat4x4.hpp>

#include <vector>


namespace mirrage::renderer {

	class Pose_comp : public ecs::Component<Pose_comp> {
	  public:
		static constexpr const char* name() { return "Pose"; }

		Pose_comp() = default;
		Pose_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		std::vector<glm::mat4> bone_transforms;
	};

	class Shared_pose_comp : public ecs::Component<Shared_pose_comp> {
	  public:
		static constexpr const char* name() { return "Shared_pose"; }

		Shared_pose_comp() = default;
		Shared_pose_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		ecs::Entity_handle pose_owner;
	};

	class Animation_comp : public ecs::Component<Animation_comp> {
	  public:
		static constexpr const char* name() { return "Animation"; }
		friend void                  load_component(ecs::Deserializer& state, Animation_comp&);
		friend void                  save_component(ecs::Serializer& state, const Animation_comp&);

		Animation_comp() = default;
		Animation_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		/// play preloaded; for small or frequently used animations
		void animation(util::Str_id, bool loop = true, bool preserve_state = false);
		/// load + play; for large one-time animations
		void animation(asset::Ptr<Animation>, bool loop = true, bool preserve_state = false);

		auto animation() { return _current_animation; }
		auto animation_id() const { return _current_animation_id; }

		auto time() const { return _time; }
		void time(float time) { _time = time; }

		auto speed() const { return _speed; }
		void speed(float s);

		auto reversed() const { return _reversed; }
		void reverse(bool r = true) { _reversed = r; }

		auto paused() const { return _paused; }
		void pause(bool p = true) { _paused = p; }

		auto looped() const { return _looped; }
		void loop(bool l = true) { _looped = l; }

		void step_time(util::Time delta_time);

	  private:
		friend class Animation_pass;

		asset::AID           _skeleton_id;
		asset::Ptr<Skeleton> _skeleton;

		std::unordered_map<util::Str_id, asset::Ptr<Animation>> _preloaded_animations;
		asset::Ptr<Animation>                                   _current_animation;
		util::maybe<util::Str_id>                               _current_animation_id;

		std::vector<Animation_key> _animation_keys;

		float _time     = 0.f;
		float _speed    = 1.f;
		bool  _reversed = false;
		bool  _paused   = false;
		bool  _looped   = true;
		bool  _dirty    = false;
	};

} // namespace mirrage::renderer
