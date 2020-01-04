#include "menu_screen.hpp"

#include "game_engine.hpp"
#include "test_animation_screen.hpp"
#include "test_screen.hpp"

#include <mirrage/gui/gui.hpp>
#include <mirrage/input/input_manager.hpp>
#include <mirrage/renderer/pass/clear_pass.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/maybe.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <thread>



namespace mirrage {
	using namespace mirrage;
	using namespace ecs::components;
	using namespace mirrage::input;
	using namespace mirrage::util::unit_literals;
	using namespace graphic;


	Menu_screen::Menu_screen(Engine& engine, bool clear_screen)
	  : Screen(engine), _mailbox(engine.bus()), _gui(&engine.gui())
	{
		if(clear_screen) {
			_renderer = dynamic_cast<Game_engine&>(engine).renderer_factory().create_renderer(
			        mirrage::util::nothing, {renderer::render_pass_id_of<renderer::Clear_pass_factory>()});

			_background = _gui->load_texture("tex:ui/background.png"_aid);
		}

		_mailbox.subscribe_to([&](Once_action& e) {
			switch(e.id) {
				case "quit"_strid: _engine.exit(); break;
			}
		});
	}
	Menu_screen::~Menu_screen() noexcept = default;

	void Menu_screen::_on_enter(mirrage::util::maybe<Screen&>) { _mailbox.enable(); }

	void Menu_screen::_on_leave(mirrage::util::maybe<Screen&>) { _mailbox.disable(); }

	void Menu_screen::_update(mirrage::util::Time dt)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(8));
		_mailbox.update_subscriptions();

		if(_renderer)
			_renderer->update(dt);
	}

	void Menu_screen::_draw()
	{
		if(_renderer)
			_renderer->draw(); //< clear screen

		if(_background)
			BackgroundImage(*_gui, _background.get(), ImGui::Image_scaling::fill_max);

		ImGui::PositionNextWindow(
		        glm::vec2(400, 600), ImGui::WindowPosition_X::center, ImGui::WindowPosition_Y::center);
		if(ImGui::Begin("##menu",
		                nullptr,
		                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
		                        | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
		                        | ImGuiWindowFlags_NoScrollWithMouse)) {

			// TODO: texture button
			if(ImGui::Button("Test Scene", ImVec2(380, 180))) {
				_engine.screens().enter<Test_screen>();
			}

			if(ImGui::Button("Animation Test Scene", ImVec2(380, 180))) {
				_engine.screens().enter<Test_animation_screen>();
			}

			//ImGui::SetCursorPos(ImVec2(150, 650));
			if(ImGui::Button("Quit", ImVec2(380, 180))) {
				_engine.exit();
			}
		}
		ImGui::End();
	}

} // namespace mirrage
