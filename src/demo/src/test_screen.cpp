#include "test_screen.hpp"

#include "game_engine.hpp"
#include "meta_system.hpp"
#include "systems/nim_system.hpp"

#include <mirrage/audio/audio_source_comp.hpp>
#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/graphic/window.hpp>
#include <mirrage/gui/gui.hpp>
#include <mirrage/input/events.hpp>
#include <mirrage/input/input_manager.hpp>
#include <mirrage/renderer/animation_comp.hpp>
#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>
#include <mirrage/renderer/pass/gui_pass.hpp>
#include <mirrage/translations.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/units.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <iomanip>
#include <sstream>


namespace mirrage {
	using namespace ecs::components;
	using namespace util::unit_literals;
	using namespace graphic;

	struct Preset {
		glm::vec3 camera_position;
		float     camera_yaw;
		float     camera_pitch;
		float     sun_elevation;
		float     sun_azimuth;
		float     sun_temperature;
		bool      disect_model;
	};

	namespace {
		constexpr auto presets = std::array<Preset, 6>{
		        {Preset{{-0.00465f, 2.693f, 0.03519f}, 0.f, 0.f, 0.92f, 1.22f, 5600.f, false},
		         Preset{{-6.2272f, 17.4041f, 0.70684f}, 1.5745f, 1.37925f, 0.64f, 1.41f, 5600.f, false},
		         Preset{{-6.92102f, 4.65626f, 8.85025f}, -4.71325f, 0.0302201f, 0.74f, 1.22f, 5600.f, true},
		         Preset{{5.93751f, 5.96643f, -4.34917f}, -0.0337765f, 0.0992601f, 0.62f, 1.22f, 5600.f, false},
		         Preset{{9.88425f, 5.69793f, 4.93024f}, 0.450757f, -0.0187274f, 0.62f, 1.85f, 5600.f, false},
		         Preset{{999.902f, 2.21469f, 7.26912f},
		                1.55188f,
		                -0.0295495f,
		                std::numeric_limits<float>::quiet_NaN(),
		                0.f,
		                5600.f,
		                false}}};
	}


	Test_screen::Test_screen(Engine& engine)
	  : Screen(engine)
	  , _mailbox(engine.bus())
	  , _meta_system(static_cast<Game_engine&>(engine))
	  , _music(engine.audio(), engine.assets().load<audio::Sample>("wav_stream:audio/music.ogg"_aid), 10.f)
	  , _gui(engine.gui())
	  , _performance_log(util::nothing)
	{
		_meta_system.entities().entity_builder("cornell").position({1000, 0, 0}).create();

		_cmd_commands.add_property(
		        "pos",
		        [&](glm::vec3 position) {
			        _camera.get<Transform_comp>().process(
			                [&](auto& transform) { transform.position = position; });
		        },
		        [&]() {
			        return _camera.get<Transform_comp>().process(
			                glm::vec3(0, 0, 0), [&](auto& transform) { return transform.position; });
		        });

		_meta_system.entities().entity_builder("sponza").create();

		_meta_system.entities().entity_builder("test_particle_emitter").position({-6, 2, 1}).create();
		_meta_system.entities().entity_builder("test_smoke_emitter").position({-6, 1, -1}).create();

		auto billboard = _meta_system.entities()
		                         .entity_builder("billboard")
		                         .position({-8, 1, 0.5f})
		                         .direction({-1, 0, 0})
		                         .create();

		_meta_system.entities().entity_builder("decal").position({-8, 0, -0.5f}).create();

		_sun = _meta_system.entities().entity_builder("sun").create();

		_camera = _meta_system.entities()
		                  .entity_builder("camera")
		                  .post_create([&](auto&&) { _set_preset(1); })
		                  .create();


		_cmd_commands.add("test.sound | Plays a test sound", [&, billboard]() mutable {
			billboard.get<audio::Audio_source_comp>().get_or_throw().play_once("example"_strid, 4.f);
		});

		_mailbox.subscribe_to([&](input::Once_action& e) {
			switch(e.id) {
				case "quit"_strid:
					if(_meta_system.nims().is_playing()) {
						_meta_system.nims().stop();
						_set_preset(1);
					} else {
						_engine.screens().leave();
					}
					break;

				case "create"_strid: {
					auto& cam = _camera.get<Transform_comp>().get_or_throw();
					_meta_system.entities()
					        .entity_builder("cube")
					        .position(cam.position + cam.direction())
					        .create();
					break;
				}

				case "print"_strid: {
					auto cam = _camera.get<Transform_comp>().get_or_throw().position;
					LOG(plog::info) << "Setup: \n"
					                << "  Camera position:    " << cam.x << "/" << cam.y << "/" << cam.z
					                << "\n"
					                << "  Camera orientation: " << _cam_yaw << "/" << _cam_pitch << "\n"
					                << "  Sun orientation:    " << _sun_elevation << "/" << _sun_azimuth
					                << "\n"
					                << "  Sun color:          " << _sun_color_temperature;
					break;
				}

				case "start_record"_strid:
					_meta_system.nims().start_recording(_current_seq);
					_record_timer = 0_s;
					break;
				case "record"_strid:
					_meta_system.nims().record(_record_timer, _current_seq);
					_record_timer = 0_s;
					break;
				case "save_record"_strid:
					_engine.assets().save("nim:demo_animation"_aid, _current_seq);
					break;
				case "playback"_strid:
					_engine.assets()
					        .load_maybe<systems::Nim_sequence>("nim:demo_animation"_aid, false)
					        .process([&](auto&& rec) {
						        _selected_preset = 0;
						        _meta_system.nims().play_looped(rec);
					        });
					break;
				case "pause"_strid:
					LOG(plog::debug) << "Pause/Unpause playback";
					_meta_system.nims().toggle_pause();
					_paused = !_paused;
					break;

				case "toggle_ui"_strid: _show_ui = !_show_ui; break;

				case "preset_a"_strid: _set_preset(1); break;
				case "preset_b"_strid: _set_preset(2); break;
				case "preset_c"_strid: _set_preset(3); break;
				case "preset_d"_strid: _set_preset(4); break;
				case "preset_e"_strid: _set_preset(5); break;
				case "preset_f"_strid: _set_preset(6); break;
			}
		});

		_mailbox.subscribe_to([&](input::Continuous_action& e) {
			switch(e.id) {
				case "quit"_strid: _engine.screens().leave(); break;

				case "capture_ptr"_strid:
					_engine.input().capture_mouse(e.begin);
					_mouse_look = e.begin;
					_set_preset(0);
					break;

				case "move_up"_strid:
					_move.z = e.begin ? -1.f : 0;
					_set_preset(0);
					break;
				case "move_down"_strid:
					_move.z = e.begin ? 1.f : 0;
					_set_preset(0);
					break;
				case "move_left"_strid:
					_move.x = e.begin ? -1.f : 0;
					_set_preset(0);
					break;
				case "move_right"_strid:
					_move.x = e.begin ? 1.f : 0;
					_set_preset(0);
					break;
			}
		});

		_mailbox.subscribe_to([&](input::Range_action& e) {
			switch(e.id) {
				case "mouse_look"_strid: _look = {e.rel.x, -e.rel.y}; break;
			}
		});
	}
	Test_screen::~Test_screen() noexcept = default;

	void Test_screen::_set_preset(int preset_id)
	{
		if(_selected_preset == preset_id) {
			return;
		}

		if(!_meta_system.nims().is_playing()) {
			_meta_system.nims().stop();
		}

		_selected_preset = preset_id;

		if(preset_id <= 0)
			return;

		const Preset& p = presets[std::size_t(preset_id - 1)];

		_camera.get<Transform_comp>().process(
		        [&](auto& transform) { transform.position = p.camera_position; });

		_cam_yaw               = p.camera_yaw;
		_cam_pitch             = p.camera_pitch;
		_sun_elevation         = p.sun_elevation;
		_sun_azimuth           = p.sun_azimuth;
		_sun_color_temperature = p.sun_temperature;

		_sun.get<renderer::Directional_light_comp>().process(
		        [&](renderer::Directional_light_comp& light) { light.temperature(_sun_color_temperature); });

		_update_sun_position();
	}

	void Test_screen::_on_enter(util::maybe<Screen&>)
	{
		_meta_system.shrink_to_fit();

		_music.unpause();
		_engine.input().enable_context("main"_strid);
		_mailbox.enable();
	}

	void Test_screen::_on_leave(util::maybe<Screen&>)
	{
		_mailbox.disable();
		_engine.input().capture_mouse(false);
		_music.pause();
	}

	void Test_screen::_update(util::Time dt)
	{
		_mailbox.update_subscriptions();

		_record_timer += dt;

		_camera.get<Transform_comp>().process([&](auto& transform) {
			if(dot(_move, _move) > 0.1f)
				transform.move_local(_move * (10_km / 1_h * dt).value());

			if(_mouse_look) {
				auto yaw   = _look.x * dt.value();
				auto pitch = -_look.y * dt.value();

				_cam_yaw = std::fmod(_cam_yaw + yaw, 2.f * glm::pi<float>());
				_cam_pitch =
				        glm::clamp(_cam_pitch + pitch, -glm::pi<float>() / 2.1f, glm::pi<float>() / 2.1f);
			}

			auto direction = glm::vec3{std::cos(_cam_pitch) * std::cos(_cam_yaw),
			                           std::sin(_cam_pitch),
			                           std::cos(_cam_pitch) * std::sin(_cam_yaw)};
			transform.look_at(transform.position - direction);
		});
		_look = {0.f, 0.f};

		_meta_system.update(_paused ? util::Time(0.0f) : dt);

		_performance_log.process([&](auto& log) {
			_performance_log_delay_left -= dt;
			if(_performance_log_delay_left.value() <= 0.f) {
				_performance_log_delay_left += 1_s;

				auto& result = _meta_system.renderer().profiler().results();

				if(_preformance_log_first_row) {
					_preformance_log_first_row = false;

					// write header
					log << result.name();
					for(auto& first_level : result) {
						log << ", " << first_level.name();

						for(auto& second_level : first_level) {
							log << ", " << second_level.name();
						}
					}

					log << "\n";
				}

				log << result.time_avg_ms();
				for(auto& first_level : result) {
					log << ", " << first_level.time_avg_ms();

					for(auto& second_level : first_level) {
						log << ", " << second_level.time_avg_ms();
					}
				}

				log << "\n" << std::flush;
			}
		});
	}


	void Test_screen::_draw()
	{
		if(_show_ui) {
			_draw_settings_window();
		}

		_meta_system.renderer().debug_draw({renderer::Debug_geometry{{0, 1, 0}, {0, 5, 0}, {1, 0, 0}},
		                                    renderer::Debug_geometry{{1, 1, 0}, {1, 5, 0}, {0, 0, 1}}});

		_meta_system.draw();
	}
	void Test_screen::_draw_settings_window()
	{
		ImGui::PositionNextWindow({250, 400}, ImGui::WindowPosition_X::left, ImGui::WindowPosition_Y::center);
		if(ImGui::Begin("Debug Controls")) {

			ImGui::TextUnformatted("Preset");
			auto preset_options = std::array<const char*, 7>{{"Free Motion",
			                                                  "Center",
			                                                  "Top-Down",
			                                                  "Side-Scroller",
			                                                  "Hallway",
			                                                  "Hallway2",
			                                                  "Cornell Box"}};

			ImGui::Combo("preset", &_selected_preset, preset_options.data(), int(preset_options.size()));


			_camera.process<renderer::Camera_comp>([&](auto& cam) {
				cam.dof_focus(ImGui::ValueSliderFloat("Focus Plane", cam.dof_focus(), 0.1f, 100.f));
				cam.dof_range(ImGui::ValueSliderFloat("Focus Range", cam.dof_range(), 0.1f, 10.f));
				cam.dof_power(ImGui::ValueSliderFloat("DOF Power", cam.dof_power(), 0.01f, 1.f));
			});

			if(!_meta_system.nims().is_playing()) {
				ImGui::TextUnformatted("Directional Light");

				auto elevation = ImGui::ValueSliderFloat("Elevation", _sun_elevation, 0.f, 1.f);
				if(std::abs(elevation - _sun_elevation) > 0.000001f) {
					_sun_elevation = elevation;
					_set_preset(0);
				}

				auto azimuth = ImGui::ValueSliderFloat("Azimuth", _sun_azimuth, -2.f, 2.f);
				if(std::abs(azimuth - _sun_azimuth) > 0.000001f) {
					_sun_azimuth = azimuth;
					_set_preset(0);
				}

				_update_sun_position();

				_sun.get<renderer::Directional_light_comp>().process(
				        [&](renderer::Directional_light_comp& light) {
					        auto new_size =
					                ImGui::ValueSliderFloat("Size", light.source_radius() / 1_m, 0.5f, 20.f);
					        light.source_radius(new_size * 1_m);

					        auto new_temp =
					                ImGui::ValueSliderFloat("Color", _sun_color_temperature, 500.f, 20000.f);

					        if(std::abs(new_temp - _sun_color_temperature) > 0.000001f) {
						        light.temperature(_sun_color_temperature = new_temp);
						        _set_preset(0);
					        }

					        ImGui::Spacing();
					        ImGui::TextUnformatted("Color");
					        auto color = util::Rgba{light.color(), light.intensity() / 50000.f};
					        if(ImGui::ColorPicker4("Color", &color.r)) {
						        light.color({color.r, color.g, color.b});
						        light.intensity(color.a * 50000.f);
						        _set_preset(0);
					        }
				        });
			}
		}
		ImGui::End();
	}

	void Test_screen::_update_sun_position()
	{
		_sun.get<Transform_comp>().process([&](auto& transform) {
			transform.orientation = glm::quat(glm::vec3(
			        (_sun_elevation - 2.f) * glm::pi<float>() / 2.f, glm::pi<float>() * _sun_azimuth, 0.f));
			transform.position    = transform.direction() * -60.f;
		});
	}
} // namespace mirrage
