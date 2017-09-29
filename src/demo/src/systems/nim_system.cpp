#include <sf2/sf2.hpp>

#include "nim_system.hpp"

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/utils/math.hpp>
#include <mirrage/utils/sf2_glm.hpp>


namespace mirrage::systems {

	using namespace util::unit_literals;

	void load_component(ecs::Deserializer& state, Nim_comp& comp) {
		state.read_virtual(sf2::vmember("uid", comp._uid));
	}

	void save_component(ecs::Serializer& state, const Nim_comp& comp) {
		state.write_virtual(sf2::vmember("uid", comp._uid));
	}

	namespace {
		struct Frame_obj_data {
			glm::vec3 position;
			glm::vec3 orientation; //< as euler angles
			glm::vec4 light_color;
		};
		struct Frame_data {
			float                                            length;
			std::unordered_map<util::Str_id, Frame_obj_data> entities;
		};

		sf2_structDef(Frame_obj_data, position, orientation, light_color);
		sf2_structDef(Frame_data, length, entities);
	} // namespace

	void load(sf2::JsonDeserializer& s, Nim_sequence& seq) {
		auto frames = std::vector<Frame_data>();

		s.read_virtual(sf2::vmember("frames", frames));

		seq._frame_lengths.clear();
		seq._affected_entities.clear();
		seq._entity_states.clear();

		if(frames.empty())
			return;

		auto entity_count = frames[0].entities.size();

		seq._frame_lengths.reserve(frames.size());
		seq._affected_entities.reserve(entity_count);
		seq._entity_states.reserve(entity_count);

		for(auto&& entity : frames[0].entities) {
			seq._affected_entities.emplace_back(entity.first);

			auto& state = seq._entity_states.emplace_back();
			state.positions.reserve(frames.size());
			state.orientations.reserve(frames.size());
			state.light_colors.reserve(frames.size());
		}

		for(auto& frame : frames) {
			seq._frame_lengths.emplace_back(frame.length * second);

			for(auto i : util::range(seq._affected_entities.size())) {
				auto& entity_uid = seq._affected_entities[i];
				auto& state      = seq._entity_states.at(i);

				auto iter = frame.entities.find(entity_uid);
				if(iter != frame.entities.end()) {
					state.positions.emplace_back(iter->second.position);
					state.orientations.emplace_back(iter->second.orientation);
					state.light_colors.emplace_back(iter->second.light_color);

				} else if(state.positions.empty()) {
					state.positions.emplace_back(0, 0, 0);
					state.orientations.emplace_back(1, 0, 0, 0);
					state.light_colors.emplace_back(1, 0.938374f, 0.88349f, 100.f);

				} else {
					state.positions.emplace_back(glm::vec3(state.positions.back()));
					state.orientations.emplace_back(glm::quat(state.orientations.back()));
					state.light_colors.emplace_back(glm::vec4(state.light_colors.back()));
				}
			}
		}
	}

	void save(sf2::JsonSerializer& s, const Nim_sequence& seq) {
		auto frames = std::vector<Frame_data>();

		frames.reserve(seq.frames());
		for(auto i : util::range(seq.frames())) {
			auto& frame  = frames.emplace_back();
			frame.length = seq._frame_lengths.at(i) / second;

			for(auto j : util::range(seq._affected_entities.size())) {
				auto& entity_state = seq._entity_states[j];
				auto  position     = entity_state.positions[i];
				auto  orientation  = glm::eulerAngles(entity_state.orientations[i]);
				auto  light_color  = entity_state.light_colors[i];
				frame.entities.emplace(seq._affected_entities[j],
				                       Frame_obj_data{position, orientation, light_color});
			}
		}

		s.write_virtual(sf2::vmember("frames", frames));
	}

	Nim_system::Nim_system(ecs::Entity_manager& ecs) : _nim_components(ecs.list<Nim_comp>()) {}

	namespace {
		template <class T>
		auto catmull_rom(float t, const std::vector<T>& points, bool closed) -> T {
			MIRRAGE_INVARIANT(!points.empty(), "Can't interpolate between zero points!");

			// calc points
			auto P1_idx = static_cast<int>(std::floor(t));
			auto P0_idx = static_cast<int>(P1_idx - 1);
			auto P2_idx = static_cast<int>(std::ceil(t));
			auto P3_idx = static_cast<int>(P2_idx + 1);

			// clamp points
			if(closed) {
				if(P0_idx < 0) {
					P0_idx = points.size() + P0_idx;
				}
				P0_idx = P0_idx % points.size();
				P1_idx = P1_idx % points.size();
				P2_idx = P2_idx % points.size();
				P3_idx = P3_idx % points.size();

			} else {
				P0_idx = util::min(P0_idx, points.size());
				P1_idx = P0_idx > 0 ? P0_idx - 1 : P0_idx;
				P2_idx = util::min(P2_idx, points.size());
				P3_idx = util::min(P3_idx, points.size());
			}

			// load points
			auto P0 = points.at(P0_idx);
			auto P1 = points.at(P1_idx);
			auto P2 = points.at(P2_idx);
			auto P3 = points.at(P3_idx);

			// calc relativ t
			auto rt  = std::fmod(t, 1.f);
			auto rt2 = rt * rt;
			auto rt3 = rt2 * rt;

			// interpolate point
			return 0.5f
			       * ((2.f * P1) + (-P0 + P2) * rt + (2.f * P0 + -5.f * P1 + 4.f * P2 + -P3) * rt2
			          + (-P0 + 3.f * P1 + -3.f * P2 + P3) * rt3);
		}
	} // namespace

	void Nim_system::update(util::Time dt) {
		if(!_playing)
			return;

		_current_position +=
		        dt / _playing->frame_length(static_cast<int>(_current_position)) * _playback_speed;

		if(_loop) {
			_current_position = std::fmod(_current_position, _playing->frames());
		}

		auto reached_end = _current_position >= _end_position && !_loop;

		if(reached_end) {
			_current_position = _end_position;
		}

		_playing->apply([&](const auto& entity_uid,
		                    const auto& positions,
		                    const auto& orientations,
		                    const auto& colors) {
			auto iter = _affected_entities.find(entity_uid);
			if(iter != _affected_entities.end()) {
				ecs::Entity_facet& entity    = iter->second;
				auto&              transform = entity.get<ecs::components::Transform_comp>().get_or_throw();

				auto idx_0 = static_cast<int>(std::floor(_current_position));
				auto idx_1 =
				        _loop ? (idx_0 + 1) % orientations.size() : util::min(idx_0 + 1, orientations.size());
				auto t           = std::fmod(_current_position, 1.f);
				auto orientation = glm::slerp(orientations[idx_0], orientations[idx_1], t);

				auto position = catmull_rom(_current_position, positions, _loop);

				auto light_color = catmull_rom(_current_position, colors, _loop);

				auto pos_diff         = glm::distance2(transform.position(), position);
				auto orientation_diff = glm::abs(glm::dot(transform.orientation(), orientation) - 1);

				if(pos_diff > 0.0001f || orientation_diff > 0.001f) {
					transform.orientation(orientation);
					transform.position(position);
				}

				entity.get<renderer::Directional_light_comp>().process([&](auto& light) {
					light.color({light_color.r, light_color.g, light_color.b});
					light.intensity(light_color.a);
				});
			}
		});

		if(reached_end) {
			stop();
		}
	}

	void Nim_system::play(Nim_sequence_ptr seq, int begin, int end, float speed) {
		if(_playing != seq) {
			_playing = seq;
		}

		_update_lookup_table();

		_playback_speed   = speed;
		_current_position = begin;
		_end_position     = end >= 0 ? end : _playing->frames();
		_loop             = false;
	}
	void Nim_system::play_looped(Nim_sequence_ptr seq, float speed) {
		play(seq, 0, -1, speed);
		_loop = true;
	}

	void Nim_system::stop() { _playing.reset(); }

	void Nim_system::start_recording(Nim_sequence& seq) {
		_update_lookup_table();

		auto affected_entities = std::vector<util::Str_id>();
		affected_entities.reserve(_affected_entities.size());
		for(auto& entity : _affected_entities) {
			affected_entities.emplace_back(entity.first);
		}

		seq = Nim_sequence(affected_entities);
	}

	void Nim_system::record(util::Time length, Nim_sequence& seq) {
		seq.push_back(length, [&](const auto& entity_uid) {
			auto iter = _affected_entities.find(entity_uid);
			if(iter == _affected_entities.end())
				return std::make_tuple(glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), util::Rgba());

			ecs::Entity_facet& entity    = iter->second;
			auto&              transform = entity.get<ecs::components::Transform_comp>().get_or_throw();

			auto color = entity.get<renderer::Directional_light_comp>().process(
			        util::Rgba(0, 0, 0, 0), [&](auto& light) {
				        return util::Rgba(
				                light.color().r, light.color().g, light.color().b, light.intensity());
			        });

			return std::make_tuple(transform.position(), transform.orientation(), color);
		});
	}

	void Nim_system::_update_lookup_table() {
		_affected_entities.clear();
		for(auto& nim_comp : _nim_components) {
			_affected_entities.emplace(nim_comp.uid(), nim_comp.owner());
		}
	}
} // namespace mirrage::systems
