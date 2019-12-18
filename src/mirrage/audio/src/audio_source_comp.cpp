#include <mirrage/audio/audio_source_comp.hpp>


namespace mirrage::audio {

	using namespace ::mirrage::audio::detail;

	namespace {
		struct Sound_state {
			util::Str_id id;
			bool         loop   = false;
			float        volume = 1.f;

			Sound_state() = default;
			Sound_state(util::Str_id id, bool loop, float volume) : id(id), loop(loop), volume(volume) {}
		};
		sf2_structDef(Sound_state, id, loop, volume);
		struct State {
			std::string                        sound_bank;
			util::small_vector<Sound_state, 4> playing_sounds;
		};
		sf2_structDef(State, sound_bank, playing_sounds);

		template <typename C>
		auto find_slot(C& container, util::Str_id id)
		{
			return std::find_if(container.begin(), container.end(), [id](auto& s) {
				return s.id == id && !is_set(s.operations, Op::stop);
			});
		}
	} // namespace

	void load_component(ecs::Deserializer& state, Audio_source_comp& comp)
	{
		State s;
		state.read(s);
		if(!s.sound_bank.empty())
			comp.sound_bank(state.assets.load<Sound_bank>(s.sound_bank));

		if(!s.playing_sounds.empty()) {
			for(auto& sound : s.playing_sounds) {
				if(sound.loop)
					comp.play_looped(sound.id, sound.volume);
				else
					comp.play_once(sound.id, sound.volume);
			}
		}
	}
	void save_component(ecs::Serializer& state, const Audio_source_comp& comp)
	{
		State s;
		if(comp._sounds)
			s.sound_bank = comp._sounds.aid().str();

		for(auto& sound : comp._slots) {
			if(!is_set(sound.operations, Op::stop)) {
				s.playing_sounds.emplace_back(sound.id, is_set(sound.operations, Op::loop), sound.volume);
			}
		}

		state.write(s);
	}

	Audio_source_comp::~Audio_source_comp()
	{
		_audio_manager.process([&](auto& am) {
			for(auto& slot : _slots) {
				if(slot.handle != 0 && is_set(slot.operations, Op::loop)) {
					am.backend().setLooping(slot.handle, false);
				}
			}
		});
	}

	void Audio_source_comp::sound_bank(asset::Ptr<Sound_bank> sb) { _sounds = sb; }

	void Audio_source_comp::play(util::Str_id id, float volume)
	{
		_slots.emplace_back(id, volume, Op::play, Op::update);
	}

	auto Audio_source_comp::play_once(util::Str_id id, float volume) -> bool
	{
		if(auto slot = find_slot(_slots, id); slot != _slots.end()) {
			unset(slot->operations, Op::loop);
			set(slot->operations, Op::update);
			slot->volume = volume;
			return false;

		} else {
			play(id, volume);
			return true;
		}
	}

	void Audio_source_comp::play_looped(util::Str_id id, float volume)
	{
		if(auto slot = find_slot(_slots, id); slot != _slots.end()) {
			set(slot->operations, Op::loop);
			set(slot->operations, Op::update);
			slot->volume = volume;

		} else {
			_slots.emplace_back(id, volume, Op::play, Op::update, Op::loop);
		}
	}
	void Audio_source_comp::stop(util::Str_id id)
	{
		for(auto& s : _slots) {
			if(s.id == id)
				set(s.operations, Op::stop);
		}
	}
	void Audio_source_comp::stop_all()
	{
		for(auto& s : _slots) {
			set(s.operations, Op::stop);
		}
	}

	auto Audio_source_comp::is_playing(util::Str_id id) const -> bool
	{
		return find_slot(_slots, id) != _slots.end();
	}

} // namespace mirrage::audio
