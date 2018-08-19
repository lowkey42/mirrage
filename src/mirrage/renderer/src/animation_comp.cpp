#include <mirrage/renderer/animation_comp.hpp>

namespace mirrage::renderer {

	void load_component(ecs::Deserializer& state, Pose_comp& a)
	{
		auto skeleton = a._skeleton_id.str();

		state.read_virtual(sf2::vmember("skeleton", skeleton));

		if(!skeleton.empty()) {
			a._skeleton_id = skeleton;
			a._skeleton    = state.assets.load<Skeleton>(skeleton);
		}
	}
	void save_component(ecs::Serializer& state, const Pose_comp& a)
	{
		state.write_virtual(sf2::vmember("skeleton", a._skeleton_id.str()));
	}


	void load(sf2::JsonDeserializer& s, Animation_state& state)
	{
		auto id       = state.animation_id.str();
		auto anim_aid = state.animation.aid().str();
		auto speed    = Animation_state::unpack_speed(state.speed);
		auto reversed = state.reversed;
		auto paused   = state.paused;
		auto looped   = state.looped;

		s.read_virtual(sf2::vmember("id", id),
		               sf2::vmember("animation", anim_aid),
		               sf2::vmember("weight", state.blend_weight),
		               sf2::vmember("time", state.time),
		               sf2::vmember("speed", speed),
		               sf2::vmember("reversed", reversed),
		               sf2::vmember("paused", paused),
		               sf2::vmember("looped", looped));

		state.animation_id = util::Str_id(id);
		if(!anim_aid.empty())
			state.animation = {asset::AID(anim_aid), {}};

		state.speed    = Animation_state::pack_speed(speed);
		state.reversed = reversed;
		state.paused   = paused;
		state.looped   = looped;
	}
	void save(sf2::JsonSerializer& s, const Animation_state& state)
	{
		auto id       = state.animation_id.str();
		auto anim_aid = state.animation.aid().str();
		auto speed    = Animation_state::unpack_speed(state.speed);
		auto reversed = state.reversed;
		auto paused   = state.paused;
		auto looped   = state.looped;

		s.write_virtual(sf2::vmember("id", id),
		                sf2::vmember("animation", anim_aid),
		                sf2::vmember("weight", state.blend_weight),
		                sf2::vmember("time", state.time),
		                sf2::vmember("speed", speed),
		                sf2::vmember("reversed", reversed),
		                sf2::vmember("paused", paused),
		                sf2::vmember("looped", looped));
	}
	void load_component(ecs::Deserializer& state, Animation_comp& a)
	{
		auto new_state = Animation_state_list{};

		state.read_virtual(sf2::vmember("state", new_state));

		if(!new_state.empty()) {
			for(auto& s : new_state) {
				s.animation = state.assets.load<Animation>(s.animation.aid());
			}
			a._animation_states = new_state;
		}
	}

	void save_component(ecs::Serializer& state, const Animation_comp& a)
	{
		state.write_virtual(sf2::vmember("state", a._animation_states));
	}


	void Animation_comp::step_time(util::Time delta_time)
	{
		for(auto& state : _animation_states) {
			if(!state.paused && state.animation) {
				auto speed = Animation_state::unpack_speed(state.speed);
				auto step  = delta_time.value() * speed * (state.reversed ? -1.f : 1.f);

				state.time += step;

				auto duration = state.animation->duration();
				if(state.looped) {
					state.time = std::fmod(state.time, duration);
					if(state.time < 0.f)
						state.time = duration + state.time;
				} else {
					state.time = glm::clamp(state.time, 0.f, duration);
				}

				_dirty = true;
			}
		}
	}

	void load_component(ecs::Deserializer& state, Simple_animation_controller_comp& a)
	{
		auto animations        = std::unordered_map<std::string, std::string>();
		auto current_animation = Animation_state{};

		state.read_virtual(sf2::vmember("animations", animations),
		                   sf2::vmember("current_animation", current_animation));

		if(!animations.empty()) {
			a._animations.clear();
			for(auto&& [key, aid] : animations) {
				a._animations.emplace(util::Str_id(key), state.assets.load<Animation>(aid));
			}
		}

		if(current_animation.animation_id) {
			util::find_maybe(a._animations, current_animation.animation_id).process([&](auto& anim) {
				current_animation.animation = anim;
				a._current_animation        = current_animation;
			});
		}
	}
	void save_component(ecs::Serializer& state, const Simple_animation_controller_comp& a)
	{
		auto animations = std::unordered_map<std::string, std::string>();
		for(auto&& [key, anim] : a._animations) {
			animations.emplace(key.str(), anim.aid().str());
		}

		if(a._current_animation.is_some()) {
			state.write_virtual(sf2::vmember("animations", animations),
			                    sf2::vmember("current_animation", a._current_animation.get_or_throw()));
		} else {
			state.write_virtual(sf2::vmember("animations", animations));
		}
	}

	void Simple_animation_controller_comp::play(util::Str_id animation, bool preserve_state)
	{
		if(preserve_state)
			play(animation, speed(), reversed(), paused(), looped());
		else
			play(animation, 1.f, false, false, true);
	}
	void Simple_animation_controller_comp::play(
	        util::Str_id animation_id, float speed, bool reversed, bool paused, bool looped)
	{
		auto& anim = [&]() -> auto&
		{
			if(_current_animation.is_nothing()
			   || _current_animation.get_or_throw().animation_id == animation_id)
				return _current_animation.get_or_throw();
			else
				return _next_animation.emplace().get_or_throw();
		}
		();

		util::find_maybe(_animations, animation_id).process([&](auto& animation) {
			anim.animation_id = animation_id;
			anim.animation    = animation;
			anim.speed        = Animation_state::pack_speed(speed);
			anim.reversed     = reversed;
			anim.paused       = paused;
			anim.looped       = looped;
		});
	}

} // namespace mirrage::renderer
