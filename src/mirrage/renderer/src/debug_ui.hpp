#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/gui/debug_ui.hpp>
#include <mirrage/gui/gui.hpp>


namespace mirrage::renderer {


	namespace {
		template <typename T>
		auto to_fixed_str(T num, int digits)
		{
			auto ss = std::stringstream{};
			ss << std::fixed << std::setprecision(digits) << num;
			return ss.str();
		}

		auto pad_left(const std::string& str, int padding)
		{
			return std::string(std::size_t(padding), ' ') + str;
		}

		template <std::size_t N, typename Container, typename Comp>
		auto top_n(const Container& container, Comp&& less)
		{
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
		auto index_of(const Container& container, const T& element) -> int
		{
			auto top_entry = std::find(container.begin(), container.end(), element);
			if(top_entry == container.end())
				return -1;

			return gsl::narrow<int>(std::distance(container.begin(), top_entry));
		}
	} // namespace


	class Deferred_renderer_factory::Profiler_menu : public gui::Debug_menu {
	  public:
		Profiler_menu(std::vector<Deferred_renderer*>& renderer_instances)
		  : Debug_menu("profiler"), _renderer_instances(renderer_instances)
		{
		}

		void on_show() override
		{
			for(auto& r : _renderer_instances)
				r->profiler().enable();
		}
		void on_hide() override
		{
			for(auto& r : _renderer_instances)
				r->profiler().disable();
		}

		void draw(gui::Gui& gui) override
		{
			for(auto& r : _renderer_instances)
				r->profiler().enable();

			auto ctx = gui.ctx();
			if(nk_begin_titled(ctx,
			                   "profiler",
			                   "Profiler",
			                   gui.centered_right(330, 380),
			                   NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE
			                           | NK_WINDOW_SCALABLE)) {

				nk_layout_row_dynamic(ctx, 20, 1);
				if(nk_button_label(ctx, "Reset")) {
					for(auto& r : _renderer_instances)
						r->profiler().reset();
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


				auto print_entry = [&](auto&&                          printer,
				                       const graphic::Profiler_result& result,
				                       int                             depth = 0,
				                       int                             rank  = -1) -> void {
					auto color = [&] {
						switch(rank) {
							case 0: return nk_rgb(255, 0, 0);
							case 1: return nk_rgb(255, 220, 128);
							default: return nk_rgb(255, 255, 255);
						}
					}();

					nk_label_colored(ctx, pad_left(result.name(), depth * 4).c_str(), NK_TEXT_LEFT, color);
					nk_label_colored(ctx, to_fixed_str(result.time_ms(), 2).c_str(), NK_TEXT_RIGHT, color);
					nk_label_colored(
					        ctx, to_fixed_str(result.time_min_ms(), 2).c_str(), NK_TEXT_RIGHT, color);
					nk_label_colored(
					        ctx, to_fixed_str(result.time_avg_ms(), 2).c_str(), NK_TEXT_RIGHT, color);
					nk_label_colored(
					        ctx, to_fixed_str(result.time_max_ms(), 2).c_str(), NK_TEXT_RIGHT, color);


					auto worst_timings = top_n<2>(result, [](auto&& lhs, auto&& rhs) {
						return lhs.time_avg_ms() < rhs.time_avg_ms();
					});

					for(auto iter = result.begin(); iter != result.end(); iter++) {
						auto rank = index_of(worst_timings, iter);
						printer(printer, *iter, depth + 1, rank);
					}
				};


				for(auto& r : _renderer_instances)
					print_entry(print_entry, r->profiler().results());
			}

			nk_end(ctx);
		}

	  private:
		std::vector<Deferred_renderer*>& _renderer_instances;
	};


	class Deferred_renderer_factory::Settings_menu : public gui::Debug_menu {
	  public:
		Settings_menu(Deferred_renderer_factory& factory, graphic::Window& window)
		  : Debug_menu("renderer_settings")
		  , _factory(factory)
		  , _window(window)
		  , _window_width(_window.width())
		  , _window_height(_window.height())
		  , _window_fullscreen(_window.fullscreen() != graphic::Fullscreen::no)
		{
		}

		void draw(gui::Gui& gui) override
		{
			auto ctx = gui.ctx();
			if(nk_begin_titled(ctx,
			                   "renderer_settings",
			                   "Renderer Settings",
			                   gui.centered_left(250, 600),
			                   NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE
			                           | NK_WINDOW_SCALABLE)) {
				nk_layout_row_dynamic(ctx, 20, 1);

				auto renderer_settings = _factory.settings();
				auto bool_nk_wrapper   = 0;

				nk_property_int(ctx, "Window Width", 640, &_window_width, 7680, 1, 1);
				nk_property_int(ctx, "Window Height", 360, &_window_height, 4320, 1, 1);

				bool_nk_wrapper = _window_fullscreen ? 1 : 0;
				nk_checkbox_label(ctx, "Fullscreen", &bool_nk_wrapper);
				_window_fullscreen = bool_nk_wrapper == 1;

				if(nk_button_label(ctx, "Apply")) {
					apply_window_changed();
				}

				nk_layout_row_dynamic(ctx, 10, 1);
				nk_label(ctx, "", NK_TEXT_LEFT);
				nk_layout_row_dynamic(ctx, 20, 1);

				nk_label(ctx, "Renderer Settings", NK_TEXT_LEFT);

				nk_layout_row_dynamic(ctx, 20, 1);
				nk_property_int(ctx, "Debug Layer", -1, &renderer_settings.debug_gi_layer, 10, 1, 1);

				bool_nk_wrapper = renderer_settings.gi ? 1 : 0;
				nk_checkbox_label(ctx, "Indirect Illumination", &bool_nk_wrapper);
				renderer_settings.gi = bool_nk_wrapper == 1;

				bool_nk_wrapper = renderer_settings.gi_shadows ? 1 : 0;
				nk_checkbox_label(ctx, "Indirect Shadows", &bool_nk_wrapper);
				renderer_settings.gi_shadows = bool_nk_wrapper == 1;

				bool_nk_wrapper = renderer_settings.gi_highres ? 1 : 0;
				nk_checkbox_label(ctx, "High-Resolution GI", &bool_nk_wrapper);
				renderer_settings.gi_highres = bool_nk_wrapper == 1;

				nk_property_int(ctx, "Minimum GI MIP", 0, &renderer_settings.gi_min_mip_level, 4, 1, 1);

				nk_property_int(ctx, "Diffuse GI MIP", 0, &renderer_settings.gi_diffuse_mip_level, 4, 1, 1);

				nk_property_int(
				        ctx, "Low-Res Sample Count", 8, &renderer_settings.gi_lowres_samples, 1024, 1, 1);

				nk_property_int(ctx, "Sample Count", 8, &renderer_settings.gi_samples, 1024, 1, 1);

				nk_property_float(
				        ctx, "Exposure", 0.f, &renderer_settings.exposure_override, 50.f, 0.001f, 0.01f);

				nk_property_float(ctx,
				                  "Background Brightness",
				                  0.f,
				                  &renderer_settings.background_intensity,
				                  10.f,
				                  1,
				                  0.1f);

				bool_nk_wrapper = renderer_settings.ssao ? 1 : 0;
				nk_checkbox_label(ctx, "Ambient Occlusion", &bool_nk_wrapper);
				renderer_settings.ssao = bool_nk_wrapper == 1;

				bool_nk_wrapper = renderer_settings.bloom ? 1 : 0;
				nk_checkbox_label(ctx, "Bloom", &bool_nk_wrapper);
				renderer_settings.bloom = bool_nk_wrapper == 1;

				bool_nk_wrapper = renderer_settings.tonemapping ? 1 : 0;
				nk_checkbox_label(ctx, "Tonemapping", &bool_nk_wrapper);
				renderer_settings.tonemapping = bool_nk_wrapper == 1;


				nk_layout_row_dynamic(ctx, 20, 2);

				if(nk_button_label(ctx, "Apply")) {
					apply_window_changed();
					_factory.settings(renderer_settings, true);
				} else {
					_factory.settings(renderer_settings, false);
				}

				if(nk_button_label(ctx, "Reset")) {
					_factory.settings(renderer::Renderer_settings{}, true);
				}
			}
			nk_end(ctx);
		}

		void apply_window_changed()
		{
			if(_window_width != _window.width() || _window_height != _window.height()
			   || _window_fullscreen != (_window.fullscreen() != graphic::Fullscreen::no)) {
				_window.dimensions(_window_width,
				                   _window_height,
				                   _window_fullscreen ? graphic::Fullscreen::yes_borderless
				                                      : graphic::Fullscreen::no);
			}
		}

	  private:
		Deferred_renderer_factory& _factory;
		graphic::Window&           _window;

		int  _window_width;
		int  _window_height;
		bool _window_fullscreen;
	};

} // namespace mirrage::renderer
