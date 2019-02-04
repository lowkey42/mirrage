/** Console and UI elements for debugging ************************************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/console_command.hpp>
#include <mirrage/utils/messagebus.hpp>

#include <iostream>
#include <unordered_set>


namespace mirrage::gui {

	class Gui;
	class Debug_ui;

	class Debug_console_appender : public plog::IAppender {
	  public:
		void write(const plog::Record& record) override;

	  private:
		friend class Debug_ui;
		struct Msg {
			plog::Severity severity;
			std::string    msg;
			Msg(plog::Severity s, std::string msg) : severity(s), msg(std::move(msg)) {}
		};

		std::vector<Msg> _messages;
	};
	inline auto& debug_console_appender()
	{
		static auto inst = Debug_console_appender();
		return inst;
	}

	class Debug_ui {
	  public:
		Debug_ui(asset::Asset_manager&, Gui&, util::Message_bus&);

		void draw();

	  private:
		static constexpr auto max_command_length = 256;

		Gui&                                 _gui;
		util::Mailbox_collection             _mailbox;
		asset::Asset_manager&                _assets;
		bool                                 _show_console        = false;
		bool                                 _focus_prompt        = false;
		bool                                 _show_suggestions    = false;
		int                                  _selected_suggestion = -1;
		std::uint32_t                        _scroll_x            = 0;
		std::uint32_t                        _scroll_y            = 0;
		bool                                 _scroll_lock         = true;
		std::array<char, max_command_length> _command_input_buffer{};
		int                                  _command_input_length = 0;
		util::Console_command_container      _commands;
		std::unordered_set<std::string>      _shown_debug_menus;
		std::vector<std::string>             _history;
		int                                  _current_history_entry = -1;

		void _save_history();
	};


	class Debug_menu {
	  public:
		Debug_menu(std::string name);
		virtual ~Debug_menu();

		virtual void draw(Gui&) = 0;
		virtual void on_show() {}
		virtual void on_hide() {}

		auto name() const noexcept -> auto& { return _name; }

		static void draw_all(const std::string& name, Gui& gui);
		static auto is_debug_menu(const std::string& name) -> bool;
		static auto print_names() -> std::string;
		static auto all_debug_menus() -> const std::vector<Debug_menu*>& { return instances(); }

	  private:
		friend class Debug_ui;
		static auto instances() -> std::vector<Debug_menu*>&
		{
			static auto list = std::vector<Debug_menu*>();
			return list;
		}

		std::string _name;
	};

} // namespace mirrage::gui
