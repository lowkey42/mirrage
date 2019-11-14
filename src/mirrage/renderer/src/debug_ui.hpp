#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/gui/debug_ui.hpp>
#include <mirrage/gui/gui.hpp>

#include <imgui.h>


namespace mirrage::renderer {

	namespace detail {
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
	} // namespace detail


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

		void draw(gui::Gui&) override
		{
			for(auto& r : _renderer_instances)
				r->profiler().enable();

			ImGui::PositionNextWindow(
			        {330, 380}, ImGui::WindowPosition_X::right, ImGui::WindowPosition_Y::center);
			if(ImGui::Begin("Renderer Profiler")) {
				if(ImGui::Button("Reset")) {
					for(auto& r : _renderer_instances)
						r->profiler().reset();
				}

#if 0
				if(_performance_log.is_nothing() && nk_button_label(ctx, "Record")) {
					_performance_log = _engine.assets().save_raw("log:perf.log"_aid);
				}
#endif

				ImGui::BeginTable("perf",
				                  {"RenderPass", "Curr (ms)", "Min (ms)", "Avg (ms)", "Max (ms)"},
				                  _first_frame);

				auto print_entry = [&](auto&&                          printer,
				                       const graphic::Profiler_result& result,
				                       int                             depth = 0,
				                       int                             rank  = -1) -> void {
					auto color = [&] {
						switch(rank) {
							case 0: return ImColor(255, 0, 0);
							case 1: return ImColor(255, 220, 128);
							default: return ImColor(255, 255, 255);
						}
					}();

					ImGui::TextColored(color, "%s", detail::pad_left(result.name(), depth * 4).c_str());
					ImGui::NextColumn();
					ImGui::TextColored(color, "%s", detail::to_fixed_str(result.time_ms(), 2).c_str());
					ImGui::NextColumn();
					ImGui::TextColored(color, "%s", detail::to_fixed_str(result.time_min_ms(), 2).c_str());
					ImGui::NextColumn();
					ImGui::TextColored(color, "%s", detail::to_fixed_str(result.time_avg_ms(), 2).c_str());
					ImGui::NextColumn();
					ImGui::TextColored(color, "%s", detail::to_fixed_str(result.time_max_ms(), 2).c_str());
					ImGui::NextColumn();

					auto worst_timings = detail::top_n<2>(result, [](auto&& lhs, auto&& rhs) {
						return lhs.time_avg_ms() < rhs.time_avg_ms();
					});

					for(auto iter = result.begin(); iter != result.end(); iter++) {
						auto rank = detail::index_of(worst_timings, iter);
						printer(printer, *iter, depth + 1, rank);
					}
				};


				for(auto& r : _renderer_instances)
					print_entry(print_entry, r->profiler().results());
			}

			ImGui::End();
			_first_frame = false;
		}

	  private:
		std::vector<Deferred_renderer*>& _renderer_instances;
		bool                             _first_frame = true;
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

		void draw(gui::Gui&) override
		{
			ImGui::PositionNextWindow(
			        {250, 600}, ImGui::WindowPosition_X::left, ImGui::WindowPosition_Y::center);
			if(ImGui::Begin("Renderer Setting")) {

				auto renderer_settings = _factory.settings();

				ImGui::SliderInt("Window Width", &_window_width, 640, 7680);
				ImGui::SliderInt("Window Height", &_window_height, 360, 4320);

				ImGui::Checkbox("Fullscreen", &_window_fullscreen);

				ImGui::Spacing();

				ImGui::TextUnformatted("Renderer Settings");

				ImGui::SliderInt("Debug Layer", &renderer_settings.debug_gi_layer, -1, 10);

				ImGui::Checkbox("Indirect Illumination", &renderer_settings.gi);

				ImGui::Checkbox("Indirect Shadows", &renderer_settings.gi_shadows);

				ImGui::Checkbox("High-Resolution GI", &renderer_settings.gi_highres);

				ImGui::SliderInt("Minimum GI MIP", &renderer_settings.gi_min_mip_level, 0, 4);

				ImGui::SliderInt("Diffuse GI MIP", &renderer_settings.gi_diffuse_mip_level, 0, 4);

				ImGui::SliderInt("Low-Res Sample Count", &renderer_settings.gi_lowres_samples, 8, 1024);
				ImGui::SliderInt("Sample Count", &renderer_settings.gi_samples, 8, 1024);

				ImGui::SliderFloat("Exposure", &renderer_settings.exposure_override, 0.f, 50.f);

				ImGui::SliderFloat("Min Disp. Lum.", &renderer_settings.min_display_luminance, 1.f, 100.f);
				ImGui::SliderFloat("Max Disp. Lum.", &renderer_settings.max_display_luminance, 1.f, 1000.f);

				ImGui::SliderFloat(
				        "Background Brightness", &renderer_settings.background_intensity, 0.f, 10.f);

				ImGui::Checkbox("Ambient Occlusion", &renderer_settings.ssao);

				ImGui::Checkbox("Bloom", &renderer_settings.bloom);

				ImGui::Checkbox("Tonemapping", &renderer_settings.tonemapping);

				ImGui::Checkbox("Depth of Field", &renderer_settings.depth_of_field);

				ImGui::TextUnformatted("Particles");
				ImGui::Checkbox("Particle Frag. Shadows", &renderer_settings.particle_fragment_shadows);
				ImGui::Checkbox("Particle GI", &renderer_settings.particle_gi);
				ImGui::Checkbox("High Quality Depth", &renderer_settings.high_quality_particle_depth);
				ImGui::SliderInt("MIP Level", &renderer_settings.transparent_particle_mip_level, 0, 5);
				ImGui::SliderInt("MIP Upsample", &renderer_settings.upsample_transparency_to_mip, 0, 5);


				if(ImGui::Button("Apply")) {
					apply_window_changed();
					_factory.settings(renderer_settings, true);
				} else {
					_factory.settings(renderer_settings, false);
				}

				if(ImGui::Button("Reset")) {
					_factory.settings(renderer::Renderer_settings{}, true);
				}
			}
			ImGui::End();
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
