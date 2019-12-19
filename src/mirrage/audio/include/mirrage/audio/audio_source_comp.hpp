#pragma once

#include <mirrage/audio/sound_bank.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/ecs/component.hpp>
#include <mirrage/utils/small_vector.hpp>

#include <glm/vec3.hpp>

namespace mirrage::audio {

	class Audio_manager;

	namespace detail {
		enum class Op : std::uint8_t {
			none   = 0,
			play   = 0b0001,
			loop   = 0b0010,
			stop   = 0b0100,
			update = 0b1000
		};
		inline void set(std::uint8_t& flags, Op op) { flags |= static_cast<std::uint8_t>(op); }
		inline void unset(std::uint8_t& flags, Op op) { flags &= ~static_cast<std::uint8_t>(op); }
		inline bool is_set(std::uint8_t flags, Op op) { return (flags & static_cast<std::uint8_t>(op)) != 0; }

		struct Slot {
			util::Str_id id;
			float        volume     = 1.f;
			unsigned int handle     = 0;
			std::uint8_t operations = 0;

			constexpr Slot() = default;
			template <typename... OPs, typename = std::enable_if_t<(std::is_same_v<Op, OPs> && ...)>>
			constexpr Slot(util::Str_id id, float volume, OPs... ops)
			  : id(id)
			  , volume(volume)
			  , handle(0)
			  , operations((std::uint8_t(0) | ... | static_cast<std::uint8_t>(ops)))
			{
			}
		};
	} // namespace detail


	class Audio_source_comp : public ecs::Component<Audio_source_comp> {
	  public:
		static constexpr const char* name() { return "Audio_source"; }
		friend void                  load_component(ecs::Deserializer& state, Audio_source_comp&);
		friend void                  save_component(ecs::Serializer& state, const Audio_source_comp&);

		using Component::Component;
		Audio_source_comp(Audio_source_comp&&) noexcept = default;
		Audio_source_comp& operator=(Audio_source_comp&&) noexcept = default;
		~Audio_source_comp();

		void sound_bank(asset::Ptr<Sound_bank>);

		/// plays sound (multiple-calls = multiple plays)
		void play(util::Str_id, float volume = 1.f);

		/// plays sound, if not already playing
		/// changes volume and looping of already playing sound
		/// returns false if already playing
		auto play_once(util::Str_id, float volume = 1.f) -> bool;

		// plays sound until stopped
		void play_looped(util::Str_id, float volume = 1.f);

		void stop(util::Str_id);
		void stop_all();

		auto is_playing(util::Str_id id) const -> bool;

	  private:
		friend class Sound_effect_system;

		util::maybe<Audio_manager&>         _audio_manager; //< used for fade-out in destructor
		asset::Ptr<Sound_bank>              _sounds;
		util::small_vector<detail::Slot, 4> _slots;
		glm::vec3                           _last_position;
	};

} // namespace mirrage::audio
