#pragma once

#include <mirrage/renderer/animation.hpp>

#include <mirrage/ecs/component.hpp>
#include <mirrage/utils/small_vector.hpp>
#include <mirrage/utils/units.hpp>

#include <vector>


namespace mirrage::renderer {

	class Pose_comp : public ecs::Component<Pose_comp> {
	  public:
		static constexpr const char* name() { return "Pose"; }
		friend void                  load_component(ecs::Deserializer& state, Pose_comp&);
		friend void                  save_component(ecs::Serializer& state, const Pose_comp&);

		Pose_comp() = default;
		Pose_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		auto bone_transforms() const -> gsl::span<const Local_bone_transform> { return _bone_transforms; }
		auto bone_transforms() -> gsl::span<Local_bone_transform> { return _bone_transforms; }

		auto skeleton() const -> const Skeleton& { return *_skeleton; }

	  private:
		friend class Animation_pass;

		asset::AID                        _skeleton_id;
		asset::Ptr<Skeleton>              _skeleton;
		std::vector<Local_bone_transform> _bone_transforms;
	};

	class Shared_pose_comp : public ecs::Component<Shared_pose_comp> {
	  public:
		static constexpr const char* name() { return "Shared_pose"; }

		Shared_pose_comp() = default;
		Shared_pose_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		ecs::Entity_handle pose_owner;
	};


	struct Animation_state {
		util::Str_id          animation_id;
		asset::Ptr<Animation> animation;
		float                 blend_weight;

		float        time;
		std::uint8_t speed;
		bool         reversed : 1;
		bool         paused : 1;
		bool         looped : 1;

		static constexpr auto pack_speed(float v)
		{
			return std::uint8_t(0.5f + (std::clamp(v / 10 * 255, 0.f, 255.f)));
		}
		static constexpr auto unpack_speed(std::uint8_t v) { return float(v) * 10 / 255; }

		Animation_state() : time(0.f), speed(pack_speed(1.f)), reversed(false), paused(false), looped(true) {}
	};

	using Animation_state_list = util::small_vector<Animation_state, 4>;

	class Animation_comp : public ecs::Component<Animation_comp> {
	  public:
		static constexpr const char* name() { return "Animation"; }
		friend void                  load_component(ecs::Deserializer& state, Animation_comp&);
		friend void                  save_component(ecs::Serializer& state, const Animation_comp&);

		Animation_comp() = default;
		Animation_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		void states(const Animation_state_list& states) { _animation_states = states; }
		auto states() -> auto& { return _animation_states; }
		auto states() const -> auto& { return _animation_states; }

		void step_time(util::Time delta_time);
		void mark_dirty() { _dirty = true; }

	  private:
		friend class Animation_pass;

		Animation_state_list _animation_states;
		bool                 _dirty = false;
	};


	class Simple_animation_controller_comp : public ecs::Component<Simple_animation_controller_comp> {
	  public:
		static constexpr const char* name() { return "Simple_animation_controller"; }
		friend void load_component(ecs::Deserializer& state, Simple_animation_controller_comp&);
		friend void save_component(ecs::Serializer& state, const Simple_animation_controller_comp&);

		Simple_animation_controller_comp() = default;
		Simple_animation_controller_comp(ecs::Entity_handle owner, ecs::Entity_manager& em)
		  : Component(owner, em)
		{
		}

		void play(util::Str_id animation, bool preserve_state = true);
		void play(util::Str_id animation, float speed, bool reversed, bool paused, bool looped);

		auto current() -> util::maybe<Animation_state&>
		{
			return _current_animation.process([](auto& v) -> Animation_state& { return v; });
		}
		auto current() const -> util::maybe<const Animation_state&>
		{
			return _current_animation.process([](auto& v) -> const Animation_state& { return v; });
		}
		auto prev() -> util::maybe<Animation_state&>
		{
			return _prev_animation.process([](auto& v) -> Animation_state& { return v; });
		}
		auto prev() const -> util::maybe<const Animation_state&>
		{
			return _prev_animation.process([](auto& v) -> const Animation_state& { return v; });
		}
		auto next() -> util::maybe<Animation_state&>
		{
			return _next_animation.process([](auto& v) -> Animation_state& { return v; });
		}
		auto next() const -> util::maybe<const Animation_state&>
		{
			return _next_animation.process([](auto& v) -> const Animation_state& { return v; });
		}


		auto animation_id() const
		{
			return current().process([&](auto& s) { return s.animation_id; });
		}
		auto animation() const -> util::maybe<asset::Ptr<Animation>>
		{
			return current().process([&](auto& s) { return s.animation; });
		}

		auto speed() const
		{
			return current().process(1.f, [](auto& s) { return Animation_state::unpack_speed(s.speed); });
		}
		void speed(float speed)
		{
			current().process([&](auto& s) { s.speed = Animation_state::pack_speed(speed); });
		}

		auto reversed() const
		{
			return current().process(false, [](auto& s) { return s.reversed; });
		}
		void reverse(bool r = true)
		{
			current().process([&](auto& s) { s.reversed = r; });
		}

		auto paused() const
		{
			return current().process(false, [](auto& s) { return s.paused; });
		}
		void pause(bool p = true)
		{
			current().process([&](auto& s) { s.paused = p; });
		}

		auto looped() const
		{
			return current().process(true, [](auto& s) { return s.looped; });
		}
		void loop(bool l = true)
		{
			current().process([&](auto& s) { s.looped = l; });
		}

		auto fade_time() const { return _fade_time; }
		void fade_time(float t) { _fade_time = t; }

	  private:
		friend class Animation_pass;

		std::unordered_map<util::Str_id, asset::Ptr<Animation>> _animations;

		util::maybe<Animation_state> _current_animation;
		util::maybe<Animation_state> _prev_animation;
		util::maybe<Animation_state> _next_animation;

		float _fade_time      = 0.25f;
		float _fade_time_left = 0.f;
	};

} // namespace mirrage::renderer
