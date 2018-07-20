#include <mirrage/renderer/animation_comp.hpp>

namespace mirrage::renderer {

	void load_component(ecs::Deserializer& state, Animation_comp& a)
	{
		auto skeleton = std::string();
		auto preload  = std::unordered_map<std::string, std::string>();

		state.read_virtual(sf2::vmember("skeleton", skeleton), sf2::vmember("animations", preload));

		a._skeleton_id = skeleton;
		a._skeleton    = state.assets.load<Skeleton>(skeleton);

		a._preloaded_animations.clear();
		for(auto&& [key, aid] : preload) {
			a._preloaded_animations.emplace(util::Str_id(key), state.assets.load<Animation>(aid));
		}
	}
	void save_component(ecs::Serializer& state, const Animation_comp& a)
	{
		auto preload = std::unordered_map<std::string, std::string>();
		for(auto&& [key, anim] : a._preloaded_animations) {
			preload.emplace(key.str(), anim.aid().str());
		}

		state.write_virtual(sf2::vmember("skeleton", a._skeleton_id.str()),
		                    sf2::vmember("animations", preload));
	}

	void Animation_comp::animation(util::Str_id id)
	{
		util::find_maybe(_preloaded_animations, id).process([&](auto& new_anim) {
			if(new_anim != _current_animation) {
				_current_animation = new_anim;
				_time              = 0.f;
			}
		});
	}
	void Animation_comp::animation(asset::AID)
	{
		// TODO
	}
} // namespace mirrage::renderer
