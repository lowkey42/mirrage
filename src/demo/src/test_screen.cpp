#include "test_screen.hpp"

#include "game_engine.hpp"
#include "meta_system.hpp"

#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>
#include <mirrage/renderer/pass/gui_pass.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/input/events.hpp>
#include <mirrage/input/input_manager.hpp>
#include <mirrage/gui/gui.hpp>
#include <mirrage/graphic/window.hpp>
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
		float camera_yaw;
		float camera_pitch;
		float sun_elevation;
		float sun_azimuth;
		float sun_temperature;
		bool  disect_model;
	};

	namespace {
		constexpr auto presets = std::array<Preset, 6> {{
				Preset{{-0.00465f,2.693f,0.03519f}, 0.f, 0.f, 0.92f, 1.22f, 5600.f, false},
				Preset{{-6.2272f,17.4041f,0.70684f}, 1.5745f, 1.37925f, 0.64f, 1.41f, 5600.f, false},
				Preset{{-6.92102f,4.65626f,8.85025f}, -4.71325f, 0.0302201f, 0.74f, 1.22f, 5600.f, true},
				Preset{{5.93751f,5.96643f,-4.34917f}, -0.0337765f, 0.0992601f, 0.62f, 1.22f, 5600.f, false},
				Preset{{9.88425f,5.69793f,4.93024f}, 0.450757f, -0.0187274f, 0.62f, 1.85f, 5600.f, false},
				Preset{{1000.29f,2.24448f,6.25397f}, 1.55188f, -0.0295495f, std::numeric_limits<float>::quiet_NaN(), 0.f, 5600.f, false}
		}};
	}


	Test_screen::Test_screen(Engine& engine)
	    : Screen(engine)
	    , _mailbox(engine.bus())
	    , _meta_system(static_cast<Game_engine&>(engine))
	    , _gui(engine.window().viewport(), engine.assets(), engine.input(),
	           _meta_system.renderer().find_pass<gui::Gui_renderer>().get_or_throw(
	               "No renderer specified to render UI elements!")) {

		_camera = _meta_system.entities().emplace("camera");

		// TODO: check if model is available
		auto cornell = _meta_system.entities().emplace("cornell");
		cornell.get<Transform_comp>().process([&](auto& transform) {
			transform.position({1000, 0, 0});
		});

		// TODO: check if model is available
		auto sponza = _meta_system.entities().emplace("sponza");
		sponza.get<Transform_comp>().process([&](auto& transform) {
			transform.scale(0.01f);
		});

		_sun = _meta_system.entities().emplace("sun");

		_set_preset(1);

		_mailbox.subscribe_to([&](input::Once_action& e){
			switch(e.id) {
				case "quit"_strid:
					_engine.screens().leave();
					break;
				case "create"_strid:
					_meta_system.entities().emplace("cube").get<Transform_comp>().process([&](auto& transform) {
						auto& cam = _camera.get<Transform_comp>().get_or_throw();
						transform.position(cam.position() + cam.direction());
					});

					break;
				case "d_disect"_strid: {
					auto s = _meta_system.renderer().settings();
					s.debug_disect = !s.debug_disect;
					_meta_system.renderer().settings(s);
					_set_preset(0);
					break;
				}

				case "print"_strid: {
					auto cam = _camera.get<Transform_comp>().get_or_throw().position();
					INFO("Setup: \n" <<
					     "  Camera position:    "<<cam.x<<"/"<<cam.y<<"/"<<cam.z<<"\n"<<
					     "  Camera orientation: "<<_cam_yaw<<"/"<<_cam_pitch<<"\n"<<
					     "  Sun orientation:    "<<_sun_elevation<<"/"<<_sun_azimuth<<"\n"<<
					     "  Sun color:          "<<_sun_color_temperature<<"\n"<<
					     "  Disected:           "<<_meta_system.renderer().settings().debug_disect);
					break;
				}

				case "preset_a"_strid:
					_set_preset(1);
					break;
				case "preset_b"_strid:
					_set_preset(2);
					break;
				case "preset_c"_strid:
					_set_preset(3);
					break;
				case "preset_d"_strid:
					_set_preset(4);
					break;
				case "preset_e"_strid:
					_set_preset(5);
					break;
				case "preset_f"_strid:
					_set_preset(6);
					break;
			}
		});

		_mailbox.subscribe_to([&](input::Continuous_action& e){
			switch(e.id) {
				case "quit"_strid:
					_engine.screens().leave();
					break;
					
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

		_mailbox.subscribe_to([&](input::Range_action& e){
			switch(e.id) {
				case "mouse_look"_strid:
					_look = {e.rel.x, -e.rel.y};
					break;
			}
		});
	}
	Test_screen::~Test_screen()noexcept = default;

	void Test_screen::_set_preset(int preset_id) {
		if(_selected_preset==preset_id) {
			return;
		}

		_selected_preset = preset_id;

		if(preset_id<=0)
			return;

		const Preset& p = presets[preset_id-1];

		_camera.get<Transform_comp>().process([&](auto& transform) {
			transform.position(p.camera_position);
		});

		_cam_yaw = p.camera_yaw;
		_cam_pitch = p.camera_pitch;
		_sun_elevation = p.sun_elevation;
		_sun_azimuth = p.sun_azimuth;
		_sun_color_temperature = p.sun_temperature;

		_sun.get<renderer::Directional_light_comp>().process([&](renderer::Directional_light_comp& light) {
			light.temperature(_sun_color_temperature);
		});

		auto s = _meta_system.renderer().settings();
		s.debug_disect = p.disect_model;
		_meta_system.renderer().settings(s);

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

		_camera.get<Transform_comp>().process([&](auto& transform) {
			if(dot(_move, _move) > 0.1f)
				transform.move_local(_move * (10_km/1_h * dt).value());
			
			if(_mouse_look) {
				auto yaw = _look.x * dt.value();
				auto pitch = -_look.y *dt.value();
				
				_cam_yaw = std::fmod(_cam_yaw+yaw, 2.f*glm::pi<float>());
				_cam_pitch = glm::clamp(_cam_pitch+pitch, -glm::pi<float>()/2.1f, glm::pi<float>()/2.1f);
			}
			
			auto direction = glm::vec3 {
				std::cos(_cam_pitch) * std::cos(_cam_yaw),
				std::sin(_cam_pitch),
				std::cos(_cam_pitch) * std::sin(_cam_yaw)
			};
			transform.look_at(transform.position() - direction);
		});
		_look = {0.f, 0.f};
		
		_meta_system.update(dt);
	}


	void Test_screen::_draw() {
		_draw_settings_window();
		_draw_profiler_window();

		_meta_system.draw();
	}
	void Test_screen::_draw_settings_window() {
		auto ctx = _gui.ctx();
		if (nk_begin_titled(ctx, "debug_controls", "Debug Controls", _gui.centered_left(400, 400),
		                    NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_TITLE|NK_WINDOW_MINIMIZABLE)) {

			nk_layout_row_dynamic(ctx, 40, 2);
			auto& renderer_settings = _meta_system.renderer().settings();
			auto dgil = nk_propertyi(ctx, "gi_layer", -1, renderer_settings.debug_gi_layer, 5, 1, 0.1);
			if(dgil!=renderer_settings.debug_gi_layer) {
				auto rs_copy = renderer_settings;
				rs_copy.debug_gi_layer = dgil;
				_meta_system.renderer().settings(rs_copy);

			}

			nk_layout_row_dynamic(ctx, 40, 2);
			nk_label(ctx, "Preset", NK_TEXT_LEFT);
			auto preset_options = std::array<const char*, 7>{{
					"Free Motion", "Center", "Top-Down", "Side-Scroller", "Hallway", "Hallway2",
					"Cornell Box"
			}};
			_set_preset(nk_combo(ctx, preset_options.data(), preset_options.size(),
			                     _selected_preset, 25,nk_vec2(200.f, 400)));


			nk_layout_row_dynamic(ctx, 40, 2);
			nk_label(ctx, "Indirect illumination", NK_TEXT_LEFT);
			int gi_active = renderer_settings.gi ? 1 : 0;
			if(nk_checkbox_label(ctx, "Active", &gi_active)) {
				auto rs_copy = renderer_settings;
				rs_copy.gi = gi_active==1;
				_meta_system.renderer().settings(rs_copy);
			}


			nk_layout_row_dynamic(ctx, 40, 1);
			nk_label(ctx, "Directional Light", NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 25, 1);

			auto elevation = nk_propertyf(ctx, "Elevation", 0.f,
			                              _sun_elevation, 1.f, 0.1f, 0.01f);
			if(elevation!=_sun_elevation) {
				_sun_elevation = elevation;
				_set_preset(0);
			}

			auto azimuth = nk_propertyf(ctx, "Azimuth", -2.f,
			                            _sun_azimuth, 2.f, 0.1f, 0.01f);
			if(azimuth!=_sun_azimuth) {
				_sun_azimuth = azimuth;
				_set_preset(0);
			}

			_update_sun_position();

			_sun.get<renderer::Directional_light_comp>().process([&](renderer::Directional_light_comp& light) {
				auto new_size = nk_propertyf(ctx, "Size", 0.5f,
				                             light.source_radius()/1_m, 20.f, 0.1f, 0.01f);
				light.source_radius(new_size*1_m);

				auto new_temp = nk_propertyf(ctx, "Color", 500.f,
				                             _sun_color_temperature, 20000.f, 500.f, 50.f);

				if(new_temp != _sun_color_temperature) {
					light.temperature(_sun_color_temperature = new_temp);
					_set_preset(0);
				}

				auto color = util::Rgba{light.color(), light.intensity()/200.f};
				if(gui::color_picker(ctx, color, 350.f)) {
					light.color({color.r, color.g, color.b});
					light.intensity(color.a*200.f);
					_set_preset(0);
				}
			});
		}
		nk_end(ctx);
	}

	namespace {
		auto to_fixed_str(double num, int digits) {
			auto ss = std::stringstream{};
			ss << std::fixed << std::setprecision(digits) << num;
			return ss.str();
		}

		auto pad_left(const std::string& str, int padding) {
			return std::string(padding, ' ') + str;
		}

		template<std::size_t N, typename Container, typename Comp>
		auto top_n(const Container& container, Comp&& less) {
			auto max_elements = std::array<decltype(container.begin()), N>();
			max_elements.fill(container.end());

			for(auto iter=container.begin(); iter!=container.end(); iter++) {
				// compare with each of the top elements
				for(auto i = std::size_t(0); i<N; i++) {
					if(max_elements[i]==container.end() || less(*max_elements[i], *iter)) {
						// move top elements to make room
						for(auto j = i+1; j<N; j++) {
							max_elements[j] = max_elements[j-1];
						}
						max_elements[i] = iter;
						break;
					}
				}
			}

			return max_elements;
		}

		template<typename Container, typename T>
		auto index_of(const Container& container, const T& element) -> int {
			auto top_entry = std::find(container.begin(), container.end(), element);
			if(top_entry == container.end())
				return -1;

			return gsl::narrow<int>(std::distance(container.begin(), top_entry));
		}
	}
	void Test_screen::_draw_profiler_window() {
		auto ctx = _gui.ctx();
		if (nk_begin_titled(ctx, "profiler", "Profiler", _gui.centered_right(600, 700),
		                    NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_TITLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_CLOSABLE)) {

			// TODO: disable when window is hidden
			_meta_system.renderer().profiler().enable();

			nk_layout_row_dynamic(ctx, 40, 1);
			if(nk_button_label(ctx, "Reset")) {
				_meta_system.renderer().profiler().reset();
			}

			constexpr auto rows = std::array<float, 5> {{0.4f, 0.15f, 0.15f, 0.15f, 0.15f}};
			nk_layout_row(ctx, NK_DYNAMIC, 50, rows.size(), rows.data());
			nk_label(ctx, "RenderPass", NK_TEXT_CENTERED);
			nk_label(ctx, "Curr (ms)",  NK_TEXT_CENTERED);
			nk_label(ctx, "Min (ms)",   NK_TEXT_CENTERED);
			nk_label(ctx, "Avg (ms)",   NK_TEXT_CENTERED);
			nk_label(ctx, "Max (ms)",   NK_TEXT_CENTERED);

			nk_layout_row(ctx, NK_DYNAMIC, 25, rows.size(), rows.data());


			auto print_entry = [&](auto&& printer, const Profiler_result& result,
			                       int depth=0, int rank=-1) -> void {

				auto color = [&] {
					switch(rank) {
						case 0:  return nk_rgb(255, 0, 0);
						case 1:  return nk_rgb(255, 220, 128);
						default: return nk_rgb(255, 255, 255);
					}
				}();

				nk_label_colored(ctx, pad_left(result.name(), depth*4).c_str(),      NK_TEXT_LEFT,  color);
				nk_label_colored(ctx, to_fixed_str(result.time_ms(),     1).c_str(), NK_TEXT_RIGHT, color);
				nk_label_colored(ctx, to_fixed_str(result.time_min_ms(), 1).c_str(), NK_TEXT_RIGHT, color);
				nk_label_colored(ctx, to_fixed_str(result.time_avg_ms(), 1).c_str(), NK_TEXT_RIGHT, color);
				nk_label_colored(ctx, to_fixed_str(result.time_max_ms(), 1).c_str(), NK_TEXT_RIGHT, color);


				auto worst_timings = top_n<2>(result, [](auto&& lhs, auto&& rhs) {
					return lhs.time_avg_ms() < rhs.time_avg_ms();
				});

				for(auto iter = result.begin(); iter!=result.end(); iter++) {
					auto rank = index_of(worst_timings, iter);
					printer(printer, *iter, depth+1, rank);
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
					(_sun_elevation - 2.f)*glm::pi<float>()/2.f,
					glm::pi<float>()*_sun_azimuth,
					0.f
			)));
			transform.position(transform.direction()*-60.f);
		});
	}

}
