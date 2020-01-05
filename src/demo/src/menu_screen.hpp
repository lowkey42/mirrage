#pragma once

#include <mirrage/engine.hpp>
#include <mirrage/gui/gui.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/utils/console_command.hpp>
#include <mirrage/utils/maybe.hpp>


namespace mirrage {

	class Menu_screen : public mirrage::Screen {
	  public:
		Menu_screen(mirrage::Engine& engine, bool clear_screen = true);
		~Menu_screen() noexcept override;

		auto name() const -> std::string override { return "Menu"; }

	  protected:
		void _update(mirrage::util::Time delta_time) override;
		void _draw() override;

		void _on_enter(mirrage::util::maybe<Screen&> prev) override;
		void _on_leave(mirrage::util::maybe<Screen&> next) override;

		auto _prev_screen_policy() const noexcept -> mirrage::Prev_screen_policy override
		{
			return mirrage::Prev_screen_policy::draw;
		}

	  protected:
		mirrage::util::Mailbox_collection                     _mailbox;
		std::unique_ptr<mirrage::renderer::Deferred_renderer> _renderer;
		mirrage::gui::Gui*                                    _gui;
		std::shared_ptr<void>                                 _background;
		std::shared_ptr<void>                                 _button;
	};

} // namespace mirrage
