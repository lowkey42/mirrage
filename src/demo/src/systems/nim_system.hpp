#pragma once

#include <mirrage/ecs/component.hpp>
#include <mirrage/ecs/entity_handle.hpp>

#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/units.hpp>

#include <sf2/sf2.hpp>

#include <glm/gtx/quaternion.hpp>
#include <glm/vec3.hpp>

#include <unordered_map>
#include <vector>


namespace mirrage::systems {

	class Nim_comp : public ecs::Component<Nim_comp> {
	  public:
		static constexpr const char* name() { return "NIM"; }
		friend void                  load_component(ecs::Deserializer& state, Nim_comp&);
		friend void                  save_component(ecs::Serializer& state, const Nim_comp&);

		Nim_comp() = default;
		Nim_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner) : Component(manager, owner) {}

		auto uid() const noexcept { return _uid; }

	  private:
		util::Str_id _uid;
	};

	class Nim_sequence {
	  public:
		friend void load(sf2::JsonDeserializer& s, Nim_sequence& e);
		friend void save(sf2::JsonSerializer& s, const Nim_sequence& e);

		Nim_sequence() = default;
		Nim_sequence(std::vector<util::Str_id> affected_entities)
		  : _affected_entities(std::move(affected_entities)), _entity_states(_affected_entities.size()) {}

		auto affected_entities() const noexcept -> auto& { return _affected_entities; }

		auto frame_length(int frame) const noexcept { return _frame_lengths[frame % _frame_lengths.size()]; }

		auto frames() const noexcept { return _frame_lengths.size(); }

		// F = void(Str_id, vector<vec3>, vector<quat>, vector<Rgba>)
		template <typename F>
		void apply(F&& callback) const {
			for(auto i : util::range(_affected_entities.size())) {
				callback(_affected_entities[i],
				         _entity_states[i].positions,
				         _entity_states[i].orientations,
				         _entity_states[i].light_colors);
			}
		}

		// source = std::tuple<vec3, quat, Rgba>(Str_id)
		template <typename F>
		void push_back(util::Time length, F&& source) {
			_frame_lengths.emplace_back(length);

			for(auto i : util::range(_affected_entities.size())) {
				auto&& state = source(_affected_entities[i]);
				_entity_states[i].positions.emplace_back(std::get<0>(state));
				_entity_states[i].orientations.emplace_back(std::get<1>(state));
				_entity_states[i].light_colors.emplace_back(std::get<2>(state));
			}
		}

	  private:
		struct Entity_frames {
			std::vector<glm::vec3>  positions;
			std::vector<glm::quat>  orientations;
			std::vector<util::Rgba> light_colors;
		};

		std::vector<util::Time>    _frame_lengths;
		std::vector<util::Str_id>  _affected_entities;
		std::vector<Entity_frames> _entity_states;
	};
	extern void load(sf2::JsonDeserializer& s, Nim_sequence& e);
	extern void save(sf2::JsonSerializer& s, const Nim_sequence& e);

	using Nim_sequence_ptr = asset::Ptr<Nim_sequence>;


	// Manages recording and playback of non-interactive-movies
	class Nim_system {
	  public:
		Nim_system(ecs::Entity_manager&);

		void update(util::Time);

		void play(Nim_sequence_ptr, int begin = 0, int end = -1, float speed = 1.f);
		void play_looped(Nim_sequence_ptr, float speed = 1.f);

		auto is_playing() const noexcept { return !!_playing; }

		void stop();

		void pause() { _paused = true; }
		void unpause() { _paused = false; }
		void toggle_pause() { _paused = !_paused; }
		auto paused() const { return _paused; }

		void start_recording(Nim_sequence&);
		// appends the current state of all relevant objects
		void record(util::Time length, Nim_sequence&);

	  private:
		using Entity_lookup_table = std::unordered_map<util::Str_id, ecs::Entity_facet>;

		Nim_comp::Pool& _nim_components;

		Entity_lookup_table _affected_entities;

		Nim_sequence_ptr _playing;
		float            _playback_speed = 0;
		float            _current_position;
		int              _end_position;
		bool             _loop;
		bool             _paused = false;

		void _update_lookup_table();
	};
} // namespace mirrage::systems
