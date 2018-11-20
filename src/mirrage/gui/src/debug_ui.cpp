#include <mirrage/gui/debug_ui.hpp>

#include <mirrage/gui/gui.hpp>

#include <mirrage/input/events.hpp>
#include <mirrage/utils/console_command.hpp>
#include <mirrage/utils/ranges.hpp>


namespace mirrage::gui {

	namespace {
		constexpr auto msg_height = 16.f;
		constexpr auto msg_color  = std::array<nk_color, 7>{{
                nk_color{255, 255, 255, 255}, //	none = 0,
                nk_color{255, 0, 0, 255},     //	fatal = 1,
                nk_color{255, 128, 0, 255},   //	error = 2,
                nk_color{255, 200, 80, 255},  //	warning = 3,
                nk_color{255, 255, 255, 255}, //	info = 4,
                nk_color{140, 140, 140, 255}, //	debug = 5,
                nk_color{140, 140, 140, 255}  //	verbose = 6
        }};
	} // namespace

	void Debug_console_appender::write(const plog::Record& record)
	{
		_messages.emplace_back(record.getSeverity(), plog::TxtFormatter::format(record));
	}


	Debug_ui::Debug_ui(Gui& gui, util::Message_bus& bus) : _gui(gui), _mailbox(bus)
	{
		_commands.add("show <ui> | Enables a specific debug UI element (see list_uis for possible options)",
		              [&](std::string ui) {
			              if(Debug_menu::is_debug_menu(ui)) {
				              _shown_debug_menus.insert(std::move(ui));
				              _show_console = false;
			              } else
				              LOG(plog::error) << "Unknown ui menu " << ui
				                               << " expected one of: " << Debug_menu::print_names;
		              });
		_commands.add("show.all | Enables all debug UI elements", [&] {
			for(auto& m : Debug_menu::all_debug_menus()) {
				_shown_debug_menus.insert(m->name());
				_show_console = false;
			}
		});
		_commands.add("hide <ui> | Disables a specific debug UI element (see list_uis for possible options)",
		              [&](std::string ui) {
			              if(Debug_menu::is_debug_menu(ui))
				              _shown_debug_menus.erase(ui);
			              else
				              LOG(plog::error) << "Unknown ui menu " << ui
				                               << " expected one of: " << Debug_menu::print_names;
		              });
		_commands.add("hide.all | Disables all debug UI elements", [&] { _shown_debug_menus.clear(); });

		_commands.add("list_uis | Lists all available debug UI elements",
		              [&]() { LOG(plog::info) << "UI menus: " << Debug_menu::print_names; });

		_mailbox.subscribe_to([&](input::Once_action& e) {
			switch(e.id) {
				case "console"_strid:
					_show_console = !_show_console;
					_focus_prompt = true;
					break;
			}
		});
	}

	void Debug_ui::draw()
	{
		_mailbox.update_subscriptions();

		for(auto& dm : _shown_debug_menus) {
			Debug_menu::draw_all(dm, _gui);
		}

		if(!_show_console)
			return;

		auto viewport = _gui.virtual_viewport();
		auto width    = viewport.z - 100;
		auto ctx      = _gui.ctx();
		if(nk_begin(ctx,
		            "debug_console",
		            nk_rect(50, 0, float(width), float(_show_suggestions ? 400 + 12 * 5 : 400)),
		            NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_DYNAMIC)) {
			nk_layout_row_dynamic(ctx, 360, 1);

			auto max_y_scroll = std::uint32_t(
			        util::max(350, msg_height * debug_console_appender()._messages.size()) - 350);

			if(_scroll_lock) {
				_scroll_y = max_y_scroll;
			}

			if(nk_group_scrolled_offset_begin(
			           ctx, &_scroll_x, &_scroll_y, "debug_console_out", NK_WINDOW_BORDER)) {
				nk_layout_row_dynamic(ctx, 12, 1);

				auto begin = int(std::floor(float(_scroll_y) / msg_height));
				auto end   = int(std::ceil(float(_scroll_y + 350) / msg_height));
				auto i     = 0;
				for(const auto& msg : debug_console_appender()._messages) {
					if(i < begin || i > end)
						nk_spacing(ctx, 1);
					else
						nk_text_colored(ctx,
						                msg.msg.c_str(),
						                int(msg.msg.size()) - 1,
						                NK_TEXT_ALIGN_LEFT,
						                msg_color[std::size_t(msg.severity)]);

					i++;
				}
			}
			nk_group_end(ctx);
			_scroll_lock = _scroll_y >= max_y_scroll;

			if(_focus_prompt) {
				nk_edit_focus(ctx, 0);
				_focus_prompt            = false;
				ctx->text_edit.cursor    = _command_input_length;
				ctx->active->edit.cursor = _command_input_length;
			}

			nk_layout_row_dynamic(ctx, 30, 1);
			auto cmd_event = nk_edit_string(ctx,
			                                NK_EDIT_FIELD | NK_EDIT_SIG_ENTER | NK_EDIT_GOTO_END_ON_ACTIVATE,
			                                _command_input_buffer.data(),
			                                &_command_input_length,
			                                max_command_length,
			                                nullptr);
			auto cmd = std::string_view(_command_input_buffer.data(), std::size_t(_command_input_length));

			if(cmd_event & NK_EDIT_COMMITED) {
				auto suggestions = util::Console_command_container::complete(cmd);

				if(_selected_suggestion >= 0 && gsl::narrow<int>(suggestions.size()) > _selected_suggestion) {
					auto s        = suggestions[std::size_t(_selected_suggestion)];
					auto name_sep = s->api().find(" ");
					auto name_len = int(name_sep == std::string::npos ? s->api().size() : name_sep);
					std::copy(s->api().begin(), s->api().begin() + name_len, _command_input_buffer.begin());
					_command_input_length = name_len;
					_focus_prompt         = true;
					_selected_suggestion  = -1;

				} else if(util::Console_command_container::call(cmd)) {
					_command_input_length = 0;
				}
			} else if(cmd_event & NK_EDIT_ACTIVE && _selected_suggestion >= 0) {
				for(auto i : util::range(static_cast<int>(NK_KEY_MAX))) {
					if(i != NK_KEY_UP && i != NK_KEY_DOWN && i != NK_KEY_ENTER
					   && nk_input_is_key_pressed(&ctx->input, static_cast<enum nk_keys>(i))) {
						_selected_suggestion = -1;
						break;
					}
				}
			}

			if(_show_suggestions) {
				auto suggestions      = util::Console_command_container::complete(cmd);
				auto suggestions_size = gsl::narrow<int>(suggestions.size());

				auto up_pressed   = nk_input_is_key_pressed(&ctx->input, NK_KEY_UP);
				auto down_pressed = nk_input_is_key_pressed(&ctx->input, NK_KEY_DOWN);

				if(up_pressed == nk_true || down_pressed == nk_true || _selected_suggestion >= 0) {
					if(up_pressed)
						_selected_suggestion--;
					else if(down_pressed)
						_selected_suggestion++;

					if(_selected_suggestion < 0)
						_selected_suggestion = suggestions_size - 1;
					else if(_selected_suggestion >= suggestions_size)
						_selected_suggestion = 0;
				}

				nk_layout_row_dynamic(ctx, 12, 3);

				auto i = -1;
				for(auto& s : suggestions) {
					i++;

					auto button_state = 0;
					auto sep          = s->api().find("|");

					auto color = _selected_suggestion == i ? nk_color{255, 255, 255, 255}
					                                       : nk_color{180, 180, 180, 180};

					auto name_sep = s->api().find(" ");
					auto name_len = int(name_sep == std::string::npos ? s->api().size() : name_sep);
					button_state |= nk_interactive_text(ctx, s->api().c_str(), name_len, color);

					if(name_sep != std::string::npos) {
						name_sep++;
						auto space = s->api().find_first_not_of(" ", name_sep);
						if(space != std::string::npos)
							name_sep = space;

						if(s->api()[name_sep] != '|')
							button_state |= nk_interactive_text(
							        ctx, s->api().c_str() + name_sep, int(sep - name_sep), color);
						else
							button_state |= nk_interactive_text(ctx, "", 0, color);

					} else {
						button_state |= nk_interactive_text(ctx, "", 0, color);
					}

					if(sep != std::string::npos) {
						sep++;
						auto space = s->api().find_first_not_of(" ", sep);
						if(space != std::string::npos)
							sep = space;
						button_state |= nk_interactive_text(
						        ctx, s->api().c_str() + sep, int(s->api().size() - sep), color);
					} else
						button_state |= nk_interactive_text(ctx, "", 0, color);

					if((button_state & NK_WIDGET_STATE_ACTIVE) && name_len > _command_input_length) {
						std::copy(
						        s->api().begin(), s->api().begin() + name_len, _command_input_buffer.begin());
						_command_input_length = name_len;
						_focus_prompt         = true;
					} else if(button_state & NK_WIDGET_STATE_HOVERED) {
						_selected_suggestion = i;
					}
				}
			}

			_show_suggestions = cmd_event & NK_EDIT_ACTIVE;
		}
		nk_end(ctx);
	}


	Debug_menu::Debug_menu(std::string name) : _name(std::move(name)) { instances().emplace_back(this); }
	Debug_menu::~Debug_menu() { util::erase_fast(instances(), this); }

} // namespace mirrage::gui
