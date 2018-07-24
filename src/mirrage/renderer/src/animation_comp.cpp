#include <mirrage/renderer/animation_comp.hpp>

namespace mirrage::renderer {

	void load_component(ecs::Deserializer& state, Animation_comp& a)
	{
		auto skeleton          = std::string();
		auto preload           = std::unordered_map<std::string, std::string>();
		auto default_animation = a._current_animation_id.get_or(""_strid).str();

		state.read_virtual(sf2::vmember("skeleton", skeleton),
		                   sf2::vmember("animations", preload),
		                   sf2::vmember("default_animation", default_animation),
		                   sf2::vmember("time", a._time),
		                   sf2::vmember("speed", a._speed),
		                   sf2::vmember("reversed", a._reversed),
		                   sf2::vmember("paused", a._paused),
		                   sf2::vmember("default_looped", a._looped));

		a._skeleton_id = skeleton;
		a._skeleton    = state.assets.load<Skeleton>(skeleton);

		a._preloaded_animations.clear();
		for(auto&& [key, aid] : preload) {
			a._preloaded_animations.emplace(util::Str_id(key), state.assets.load<Animation>(aid));
		}

		a.animation(util::Str_id(default_animation), a._looped, true);
	}
	void save_component(ecs::Serializer& state, const Animation_comp& a)
	{
		auto preload = std::unordered_map<std::string, std::string>();
		for(auto&& [key, anim] : a._preloaded_animations) {
			preload.emplace(key.str(), anim.aid().str());
		}

		auto default_animation = a._current_animation_id.get_or(""_strid).str();

		state.write_virtual(sf2::vmember("skeleton", a._skeleton_id.str()),
		                    sf2::vmember("animations", preload),
		                    sf2::vmember("default_animation", default_animation),
		                    sf2::vmember("time", a._time),
		                    sf2::vmember("speed", a._speed),
		                    sf2::vmember("reversed", a._reversed),
		                    sf2::vmember("paused", a._paused),
		                    sf2::vmember("default_looped", a._looped));
	}

	void Animation_comp::animation(util::Str_id id, bool loop, bool preserve_state)
	{
		if(auto anim = util::find_maybe(_preloaded_animations, id); anim.is_some()) {
			_current_animation_id = id;
			animation(anim.get_or_throw(), loop, preserve_state);

		} else if(_current_animation) {
			animation(asset::Ptr<Animation>{}, preserve_state);
		}
	}
	void Animation_comp::animation(asset::Ptr<Animation> anim, bool loop, bool preserve_state)
	{
		_looped = loop;

		if(anim) {
			if(anim != _current_animation) {
				_current_animation = anim;
				if(!preserve_state) {
					_time     = 0.f;
					_speed    = 1.f;
					_reversed = false;
					_paused   = false;
					_animation_keys.clear();
				}
			}

		} else if(_current_animation) {
			_current_animation_id = util::nothing;
			_current_animation    = {};
			_time                 = 0.f;
			_animation_keys.clear();
		}
	}

	void Animation_comp::speed(float s)
	{
		_speed = std::abs(s);
		_dirty = true;
	}

	void Animation_comp::step_time(util::Time delta_time)
	{
		if(_paused || !_current_animation)
			return;

		auto step = delta_time.value() * _speed * (_reversed ? -1.f : 1.f);

		_time += step;

		auto duration = _current_animation->duration();
		if(_looped) {
			_time = std::fmod(_time, duration);
			if(_time < 0.f)
				_time = duration + _time;
		} else {
			_time = glm::clamp(_time, 0.f, duration);
		}

		_dirty = true;
	}

} // namespace mirrage::renderer
