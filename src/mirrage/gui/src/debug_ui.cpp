#include <mirrage/gui/debug_ui.hpp>

#include <mirrage/gui/gui.hpp>

#include <mirrage/input/events.hpp>
#include <mirrage/utils/console_command.hpp>


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
		_commands.add("show <ui>", [&](std::string ui) {
			if(Debug_menu::is_debug_menu(ui)) {
				_shown_debug_menus.insert(std::move(ui));
				_show_console = false;
			} else
				LOG(plog::error) << "Unknown ui menu " << ui
				                 << " expected one of: " << Debug_menu::print_names;
		});
		_commands.add("hide <ui>", [&](std::string ui) {
			if(Debug_menu::is_debug_menu(ui))
				_shown_debug_menus.erase(ui);
			else
				LOG(plog::error) << "Unknown ui menu " << ui
				                 << " expected one of: " << Debug_menu::print_names;
		});
		_commands.add("list_uis", [&]() { LOG(plog::info) << "UI menus: " << Debug_menu::print_names; });

		_mailbox.subscribe_to([&](input::Once_action& e) {
			switch(e.id) {
				case "console"_strid:
					_show_console         = !_show_console;
					_show_console_changed = true;
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
		if(nk_begin(ctx, "debug_console", nk_rect(50, 0, float(width), float(400)), NK_WINDOW_NO_SCROLLBAR)) {
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

			if(_show_console_changed) {
				_show_console_changed = false;
				nk_edit_focus(ctx, 0);
			}

			nk_layout_row_dynamic(ctx, 30, 1);
			auto cmd_event = nk_complete_begin(
			        ctx, _command_input_buffer.data(), &_command_input_length, max_command_length);
			auto cmd = std::string_view(_command_input_buffer.data(), std::size_t(_command_input_length));

			if(cmd_event & NK_EDIT_COMMITED) {
				if(util::Console_command_container::call(cmd)) {
					_command_input_length = 0;
				}
			}
			if(cmd_event & NK_EDIT_ACTIVE) {
				nk_layout_row_dynamic(ctx, 12, 1);

				for(auto& s : util::Console_command_container::complete(cmd)) {
					nk_label(ctx, s->api().c_str(), NK_TEXT_LEFT);
				}

				nk_complete_end(ctx);
			}
		}
		nk_end(ctx);
	}


	Debug_menu::Debug_menu(std::string name) : _name(std::move(name)) { instances().emplace_back(this); }
	Debug_menu::~Debug_menu() { util::erase_fast(instances(), this); }

} // namespace mirrage::gui
