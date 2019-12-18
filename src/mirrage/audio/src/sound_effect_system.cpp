#include <mirrage/audio/sound_effect_system.hpp>

#include <mirrage/audio/audio_source_comp.hpp>
#include <mirrage/audio/listener_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/entity_set_view.hpp>
#include <mirrage/utils/container_utils.hpp>


using mirrage::ecs::components::Transform_comp;

namespace mirrage::audio {

	using detail::Op;

	namespace {
		constexpr auto stop_fade_out_time = 0.25f;
	}

	Sound_effect_system::Sound_effect_system(ecs::Entity_manager& ecs, Audio_manager& audio)
	  : _ecs(ecs), _audio(audio), _bus(), _rand(util::construct_random_engine())
	{
		_bus_handle = _audio.backend().play(_bus);

		_ecs.register_component_type<Listener_comp>();
		_ecs.register_component_type<Audio_source_comp>();
	}

	void Sound_effect_system::pause()
	{
		_audio.backend().fadeVolume(_bus_handle, 0, 0.5f);
		_audio.backend().schedulePause(_bus_handle, 0.5f);
	}
	void Sound_effect_system::unpause()
	{
		_audio.backend().setPause(_bus_handle, false);
		_audio.backend().fadeVolume(_bus_handle, 1.f, 0.5f);
	}

	void Sound_effect_system::play(Sample_ptr sample, float volume, ecs::Entity_handle entity)
	{
		_play(std::move(sample), volume * _audio.settings().gameplay_volume, entity);
	}
	void Sound_effect_system::play_dialog(Sample_ptr sample, float volume, ecs::Entity_handle entity)
	{
		auto h = _play(std::move(sample), volume * _audio.settings().dialog_volume, entity);
		_audio.backend().setInaudibleBehavior(h, true, false);
	}

	auto Sound_effect_system::_play(Sample_ptr sample, float volume, ecs::Entity_handle entity)
	        -> unsigned int
	{
		auto handle = static_cast<unsigned int>(0);

		if(sample && *sample) {
			_ecs.process(entity, [&](Audio_source_comp& source, Transform_comp& transform) {
				handle = _bus.play3dClocked(_audio.time(),
				                            **sample,
				                            transform.position.x,
				                            transform.position.y,
				                            transform.position.z,
				                            0.f,
				                            0.f,
				                            0.f,
				                            volume);

				source._slots.emplace_back(util::Str_id{}, volume);
				source._slots.back().handle = handle;
			});

			if(handle == 0) {
				handle = _bus.playClocked(_audio.time(), **sample, volume, 0.f);
			}
		}

		return handle;
	}

	void Sound_effect_system::update(util::Time dt)
	{
		if(!_audio.backend().isValidVoiceHandle(_bus_handle)) {
			LOG(plog::warning) << "Sound_effect_system bus not valid";
			return;
		}

		auto master_volume = _audio.settings().gameplay_volume;

		// find listener
		auto listener_position    = util::maybe<glm::vec3>::nothing();
		auto listener_orientation = util::maybe<glm::quat>::nothing();
		auto listener_priority    = std::numeric_limits<float>::lowest();
		for(auto& [listener, transform] : _ecs.list<Listener_comp, Transform_comp>()) {
			if(listener.priority >= listener_priority) {
				listener_priority    = listener.priority;
				listener_position    = transform.position + listener.offset;
				listener_orientation = transform.orientation;
			}
		}

		// update listener position
		process(listener_position, listener_orientation) >> [&](const glm::vec3& pos, const glm::quat& o) {
			auto vel = _last_listener_position.process(glm::vec3(0, 0, 0),
			                                           [&](auto& lp) { return (pos - lp) / dt.value(); });

			auto at = glm::rotate(o, glm::vec3(0, 0, -1));
			auto up = glm::rotate(o, glm::vec3(0, 1, 0));

			_audio.backend().set3dListenerParameters(
			        pos.x, pos.y, pos.z, at.x, at.y, at.z, up.x, up.y, up.z, vel.x, vel.y, vel.z);
		};
		_last_listener_position = _last_listener_position;

		// update sound effect
		for(auto& [source, transform] : _ecs.list<Audio_source_comp, Transform_comp>()) {
			source._audio_manager = _audio;

			const auto pos        = transform.position;
			const auto vel        = (pos - source._last_position) / dt.value();
			source._last_position = pos;

			for(auto& slot : source._slots) {
				if(!_audio.backend().isValidVoiceHandle(slot.handle)) {
					slot.handle = 0;

				} else if(is_set(slot.operations, Op::stop)) {
					_audio.backend().fadeVolume(slot.handle, 0, stop_fade_out_time);
					_audio.backend().scheduleStop(slot.handle, stop_fade_out_time);
					slot.handle = 0;

				} else {
					_audio.backend().set3dSourceParameters(
					        slot.handle, pos.x, pos.y, pos.z, vel.x, vel.y, vel.z);

					if(is_set(slot.operations, Op::update)) {
						_audio.backend().setLooping(slot.handle, is_set(slot.operations, Op::loop));
						_audio.backend().setVolume(slot.handle, slot.volume * master_volume);
						unset(slot.operations, Op::update);
					}
				}

				if(slot.handle == 0 && is_set(slot.operations, Op::play)) {
					auto sample = source._sounds->get(slot.id, _rand);
					if(sample && *sample) {
						slot.handle = _bus.play3dClocked(_audio.time(),
						                                 **sample,
						                                 pos.x,
						                                 pos.y,
						                                 pos.z,
						                                 vel.x,
						                                 vel.y,
						                                 vel.z,
						                                 slot.volume * master_volume);

						if(!_audio.backend().isValidVoiceHandle(slot.handle)) {
							LOG(plog::error) << "play failed for effect: " << slot.id.str();
						}

						_audio.backend().setLooping(slot.handle, is_set(slot.operations, Op::loop));
						unset(slot.operations, Op::play);
					}
				}
			}

			util::erase_if(source._slots, [](auto& s) { return s.handle == 0; });
		}

		_audio.backend().update3dAudio();
	}

} // namespace mirrage::audio
