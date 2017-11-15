#include "test_screen.hpp"

#include "game_engine.hpp"
#include "meta_system.hpp"
#include "systems/nim_system.hpp"

#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>
#include <mirrage/renderer/pass/gui_pass.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/graphic/window.hpp>
#include <mirrage/gui/gui.hpp>
#include <mirrage/input/events.hpp>
#include <mirrage/input/input_manager.hpp>
#include <mirrage/translations.hpp>
#include <mirrage/utils/units.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
	  , _gui(engine.window().viewport(),
	         engine.assets(),
	         engine.input(),
	         _meta_system.renderer().find_pass<gui::Gui_renderer>())
	  , _performance_log(util::nothing)
	  , _window_width(engine.window().width())
	  , _window_height(engine.window().height())
	  , _window_fullscreen(engine.window().fullscreen() != graphic::Fullscreen::no) {

		_camera = _meta_system.entities().emplace("camera");

		auto cornell = _meta_system.entities().emplace("cornell");
		cornell.get<Transform_comp>().process([&](auto& transform) { transform.position({1000, 0, 0}); });

		_meta_system.entities().emplace("sponza");

		_sun = _meta_system.entities().emplace("sun");

		_set_preset(1);

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
				case "fast_quit"_strid: std::terminate(); break;
				case "create"_strid:
					_meta_system.entities().emplace("cube").get<Transform_comp>().process(
					        [&](auto& transform) {
						        auto& cam = _camera.get<Transform_comp>().get_or_throw();
						        transform.position(cam.position() + cam.direction());
					        });

					break;
				case "d_disect"_strid: {
					auto s         = _meta_system.renderer().settings();
					s.debug_disect = !s.debug_disect;
					_meta_system.renderer().settings(s, false);
					_set_preset(0);
					break;
				}

				case "print"_strid: {
					auto cam = _camera.get<Transform_comp>().get_or_throw().position();
					MIRRAGE_INFO("Setup: \n"
					             << "  Camera position:    " << cam.x << "/" << cam.y << "/" << cam.z << "\n"
					             << "  Camera orientation: " << _cam_yaw << "/" << _cam_pitch << "\n"
					             << "  Sun orientation:    " << _sun_elevation << "/" << _sun_azimuth << "\n"
					             << "  Sun color:          " << _sun_color_temperature << "\n"
					             << "  Disected:           "
					             << _meta_system.renderer().settings().debug_disect);
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
					MIRRAGE_INFO("Pause/Unpause playback");
					_meta_system.nims().toggle_pause();
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

				case "capture_mouse"_strid:
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

	void Test_screen::_set_preset(int preset_id) {
		if(_selected_preset == preset_id) {
			return;
		}

		if(!_meta_system.nims().is_playing()) {
			_meta_system.nims().stop();
		}

		_selected_preset = preset_id;

		if(preset_id <= 0)
			return;

		const Preset& p = presets[preset_id - 1];

		_camera.get<Transform_comp>().process(
		        [&](auto& transform) { transform.position(p.camera_position); });

		_cam_yaw               = p.camera_yaw;
		_cam_pitch             = p.camera_pitch;
		_sun_elevation         = p.sun_elevation;
		_sun_azimuth           = p.sun_azimuth;
		_sun_color_temperature = p.sun_temperature;

		_sun.get<renderer::Directional_light_comp>().process(
		        [&](renderer::Directional_light_comp& light) { light.temperature(_sun_color_temperature); });

		auto s         = _meta_system.renderer().settings();
		s.debug_disect = p.disect_model;
		_meta_system.renderer().settings(s, false);

		_update_sun_position();
	}

	void Test_screen::_on_enter(util::maybe<Screen&> prev) {
		_meta_system.shrink_to_fit();

		_engine.input().enable_context("main"_strid);
		_mailbox.enable();
	}

	void Test_screen::_on_leave(util::maybe<Screen&> next) {
		_mailbox.disable();
		_engine.input().capture_mouse(false);
	}

	void Test_screen::_update(util::Time dt) {
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
			transform.look_at(transform.position() - direction);
		});
		_look = {0.f, 0.f};

		_meta_system.update(dt);

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


	void Test_screen::_draw() {
		if(_show_ui) {
			_draw_settings_window();

			if(_show_profiler) {
				_meta_system.renderer().profiler().enable();
				_draw_profiler_window();
			} else {
				_meta_system.renderer().profiler().disable();
			}
		}

		_meta_system.draw();
	}
	void Test_screen::_draw_settings_window() {
		auto ctx = _gui.ctx();
		if(nk_begin_titled(ctx,
		                   "debug_controls",
		                   "Debug Controls",
		                   _gui.centered_left(250, 720),
		                   NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE)) {

			nk_layout_row_dynamic(ctx, 20, 2);

			nk_label(ctx, "Preset", NK_TEXT_LEFT);
			auto preset_options = std::array<const char*, 7>{{"Free Motion",
			                                                  "Center",
			                                                  "Top-Down",
			                                                  "Side-Scroller",
			                                                  "Hallway",
			                                                  "Hallway2",
			                                                  "Cornell Box"}};
			_set_preset(nk_combo(ctx,
			                     preset_options.data(),
			                     preset_options.size(),
			                     _selected_preset,
			                     14,
			                     nk_vec2(100.f, 200)));


			nk_layout_row_dynamic(ctx, 20, 1);
			auto show_profiler = _show_profiler ? 1 : 0;
			if(nk_checkbox_label(ctx, "Show Profiler", &show_profiler)) {
				_show_profiler = show_profiler == 1;
			}


			if(!_meta_system.nims().is_playing()) {
				nk_layout_row_dynamic(ctx, 20, 1);
				nk_label(ctx, "Directional Light", NK_TEXT_LEFT);

				nk_layout_row_dynamic(ctx, 14, 1);

				auto elevation = nk_propertyf(ctx, "Elevation", 0.f, _sun_elevation, 1.f, 0.05f, 0.001f);
				if(elevation != _sun_elevation) {
					_sun_elevation = elevation;
					_set_preset(0);
				}

				auto azimuth = nk_propertyf(ctx, "Azimuth", -2.f, _sun_azimuth, 2.f, 0.05f, 0.001f);
				if(azimuth != _sun_azimuth) {
					_sun_azimuth = azimuth;
					_set_preset(0);
				}

				_update_sun_position();

				_sun.get<renderer::Directional_light_comp>().process(
				        [&](renderer::Directional_light_comp& light) {
					        auto new_size = nk_propertyf(
					                ctx, "Size", 0.5f, light.source_radius() / 1_m, 20.f, 0.1f, 0.01f);
					        light.source_radius(new_size * 1_m);

					        auto new_temp = nk_propertyf(
					                ctx, "Color", 500.f, _sun_color_temperature, 20000.f, 500.f, 50.f);

					        if(new_temp != _sun_color_temperature) {
						        light.temperature(_sun_color_temperature = new_temp);
						        _set_preset(0);
					        }

					        auto color = util::Rgba{light.color(), light.intensity() / 200.f};
					        if(gui::color_picker(ctx, color, 210.f)) {
						        light.color({color.r, color.g, color.b});
						        light.intensity(color.a * 200.f);
						        _set_preset(0);
					        }
				        });
			}

			nk_layout_row_dynamic(ctx, 10, 1);
			nk_label(ctx, "", NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 20, 1);
			nk_label(ctx, "Graphic Settings", NK_TEXT_LEFT);
			auto renderer_settings = _meta_system.renderer().settings();
			auto bool_nk_wrapper   = 0;

			nk_property_int(ctx, "Window Width", 640, &_window_width, 7680, 1, 1);
			nk_property_int(ctx, "Window Height", 360, &_window_height, 4320, 1, 1);

			bool_nk_wrapper = _window_fullscreen ? 1 : 0;
			nk_checkbox_label(ctx, "Fullscreen", &bool_nk_wrapper);
			_window_fullscreen = bool_nk_wrapper == 1;

			if(nk_button_label(ctx, "Apply")) {
				if(_window_width != _engine.window().width() || _window_height != _engine.window().height()
				   || _window_fullscreen != (_engine.window().fullscreen() != graphic::Fullscreen::no)) {
					_engine.window().dimensions(_window_width,
					                            _window_height,
					                            _window_fullscreen ? graphic::Fullscreen::yes_borderless
					                                               : graphic::Fullscreen::no);
				}
			}

			nk_layout_row_dynamic(ctx, 10, 1);
			nk_label(ctx, "", NK_TEXT_LEFT);
			nk_layout_row_dynamic(ctx, 20, 1);

			nk_label(ctx, "Renderer Settings", NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 20, 1);
			nk_property_int(ctx, "Debug Layer", -1, &renderer_settings.debug_gi_layer, 5, 1, 1);

			bool_nk_wrapper = renderer_settings.gi ? 1 : 0;
			nk_checkbox_label(ctx, "Indirect Illumination", &bool_nk_wrapper);
			renderer_settings.gi = bool_nk_wrapper == 1;

			bool_nk_wrapper = renderer_settings.gi_highres ? 1 : 0;
			nk_checkbox_label(ctx, "High-Resolution GI", &bool_nk_wrapper);
			renderer_settings.gi_highres = bool_nk_wrapper == 1;

			nk_property_int(ctx, "Minimum GI MIP", 0, &renderer_settings.gi_min_mip_level, 4, 1, 1);

			nk_property_int(ctx, "Diffuse GI MIP", 0, &renderer_settings.gi_diffuse_mip_level, 4, 1, 1);

			nk_property_int(ctx, "Sample Count", 8, &renderer_settings.gi_samples, 256, 1, 1);

			nk_property_float(ctx,
			                  "Prioritise Near Samples",
			                  0.f,
			                  &renderer_settings.gi_prioritise_near_samples,
			                  1.f,
			                  0.1,
			                  0.01);

			nk_property_int(
			        ctx, "Low-Quality MIP-Levels", 0, &renderer_settings.gi_low_quality_mip_levels, 8, 1, 1);

			nk_property_float(ctx, "Exposure", 0.f, &renderer_settings.exposure_override, 50.f, 0.01, 0.1);

			nk_property_float(
			        ctx, "Background Brightness", 0.f, &renderer_settings.background_intensity, 10.f, 1, 0.1);

			bool_nk_wrapper = renderer_settings.ssao ? 1 : 0;
			nk_checkbox_label(ctx, "Ambient Occlusion", &bool_nk_wrapper);
			renderer_settings.ssao = bool_nk_wrapper == 1;

			bool_nk_wrapper = renderer_settings.bloom ? 1 : 0;
			nk_checkbox_label(ctx, "Bloom", &bool_nk_wrapper);
			renderer_settings.bloom = bool_nk_wrapper == 1;


			nk_layout_row_dynamic(ctx, 20, 2);

			if(nk_button_label(ctx, "Apply")) {
				if(_window_width != _engine.window().width() || _window_height != _engine.window().height()
				   || _window_fullscreen != (_engine.window().fullscreen() != graphic::Fullscreen::no)) {
					_engine.window().dimensions(_window_width,
					                            _window_height,
					                            _window_fullscreen ? graphic::Fullscreen::yes_borderless
					                                               : graphic::Fullscreen::no);
				}
				_meta_system.renderer().settings(renderer_settings, true);
			} else {
				_meta_system.renderer().settings(renderer_settings, false);
			}

			if(nk_button_label(ctx, "Reset")) {
				_meta_system.renderer().settings(renderer::Renderer_settings{}, true);
			}
		}
		nk_end(ctx);
	}

	namespace {
		auto to_fixed_str(double num, int digits) {
			auto ss = std::stringstream{};
			ss << std::fixed << std::setprecision(digits) << num;
			return ss.str();
		}

		auto pad_left(const std::string& str, int padding) { return std::string(padding, ' ') + str; }

		template <std::size_t N, typename Container, typename Comp>
		auto top_n(const Container& container, Comp&& less) {
			auto max_elements = std::array<decltype(container.begin()), N>();
			max_elements.fill(container.end());

			for(auto iter = container.begin(); iter != container.end(); iter++) {
				// compare with each of the top elements
				for(auto i = std::size_t(0); i < N; i++) {
					if(max_elements[i] == container.end() || less(*max_elements[i], *iter)) {
						// move top elements to make room
						for(auto j = i + 1; j < N; j++) {
							max_elements[j] = max_elements[j - 1];
						}
						max_elements[i] = iter;
						break;
					}
				}
			}

			return max_elements;
		}

		template <typename Container, typename T>
		auto index_of(const Container& container, const T& element) -> int {
			auto top_entry = std::find(container.begin(), container.end(), element);
			if(top_entry == container.end())
				return -1;

			return gsl::narrow<int>(std::distance(container.begin(), top_entry));
		}
	} // namespace
	void Test_screen::_draw_profiler_window() {
		auto ctx = _gui.ctx();
		if(nk_begin_titled(ctx,
		                   "profiler",
		                   "Profiler",
		                   _gui.centered_right(330, 380),
		                   NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE)) {

			nk_layout_row_dynamic(ctx, 20, 1);
			if(nk_button_label(ctx, "Reset")) {
				_meta_system.renderer().profiler().reset();
			}

#if 0
			if(_performance_log.is_nothing() && nk_button_label(ctx, "Record")) {
				_performance_log = _engine.assets().save_raw("log:perf.log"_aid);
			}
#endif

			constexpr auto rows = std::array<float, 5>{{0.4f, 0.15f, 0.15f, 0.15f, 0.15f}};
			nk_layout_row(ctx, NK_DYNAMIC, 25, rows.size(), rows.data());
			nk_label(ctx, "RenderPass", NK_TEXT_CENTERED);
			nk_label(ctx, "Curr (ms)", NK_TEXT_CENTERED);
			nk_label(ctx, "Min (ms)", NK_TEXT_CENTERED);
			nk_label(ctx, "Avg (ms)", NK_TEXT_CENTERED);
			nk_label(ctx, "Max (ms)", NK_TEXT_CENTERED);

			nk_layout_row(ctx, NK_DYNAMIC, 10, rows.size(), rows.data());


			auto print_entry =
			        [&](auto&& printer, const Profiler_result& result, int depth = 0, int rank = -1) -> void {

				auto color = [&] {
					switch(rank) {
						case 0: return nk_rgb(255, 0, 0);
						case 1: return nk_rgb(255, 220, 128);
						default: return nk_rgb(255, 255, 255);
					}
				}();

				nk_label_colored(ctx, pad_left(result.name(), depth * 4).c_str(), NK_TEXT_LEFT, color);
				nk_label_colored(ctx, to_fixed_str(result.time_ms(), 1).c_str(), NK_TEXT_RIGHT, color);
				nk_label_colored(ctx, to_fixed_str(result.time_min_ms(), 1).c_str(), NK_TEXT_RIGHT, color);
				nk_label_colored(ctx, to_fixed_str(result.time_avg_ms(), 1).c_str(), NK_TEXT_RIGHT, color);
				nk_label_colored(ctx, to_fixed_str(result.time_max_ms(), 1).c_str(), NK_TEXT_RIGHT, color);


				auto worst_timings = top_n<2>(
				        result, [](auto&& lhs, auto&& rhs) { return lhs.time_avg_ms() < rhs.time_avg_ms(); });

				for(auto iter = result.begin(); iter != result.end(); iter++) {
					auto rank = index_of(worst_timings, iter);
					printer(printer, *iter, depth + 1, rank);
				}
			};

			auto& result = _meta_system.renderer().profiler().results();
			print_entry(print_entry, result);
		}

		nk_end(ctx);
	}

	void Test_screen::_update_sun_position() {
		_sun.get<Transform_comp>().process([&](auto& transform) {
			transform.orientation(glm::quat(glm::vec3(
			        (_sun_elevation - 2.f) * glm::pi<float>() / 2.f, glm::pi<float>() * _sun_azimuth, 0.f)));
			transform.position(transform.direction() * -60.f);
		});
	}
} // namespace mirrage
