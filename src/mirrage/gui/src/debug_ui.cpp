#include <mirrage/gui/debug_ui.hpp>

#include <mirrage/gui/gui.hpp>

#include <mirrage/input/events.hpp>
#include <mirrage/utils/console_command.hpp>
#include <mirrage/utils/ranges.hpp>

#include <imgui.h>


template <class = void>
void quick_exit(int) noexcept
{
	std::abort();
}
void mirrage_quick_exit() noexcept
{
	using namespace std;
	// calls std::quick_exit if it exists or the template-fallback defined above, if not
	// needs to be at global scope for this workaround to work correctly
	quick_exit(0);
}

namespace mirrage::gui {

	namespace {
		const auto msg_color = std::array<ImVec4, 7>{{
		        ImVec4{255, 255, 255, 255}, //	none = 0,
		        ImVec4{255, 0, 0, 255},     //	fatal = 1,
		        ImVec4{255, 128, 0, 255},   //	error = 2,
		        ImVec4{255, 200, 80, 255},  //	warning = 3,
		        ImVec4{255, 255, 255, 255}, //	info = 4,
		        ImVec4{140, 140, 140, 255}, //	debug = 5,
		        ImVec4{140, 140, 140, 255}  //	verbose = 6
		}};

		const auto history_aid = "cfg:console_history"_aid;
	} // namespace

	void Debug_console_appender::write(const plog::Record& record)
	{
		auto msg =
#ifdef _WIN32
		        plog::util::toNarrow(plog::TxtFormatter::format(record), 0);
#else
		        plog::TxtFormatter::format(record);
#endif
		auto prev_pos = std::string::size_type(0);
		auto pos      = std::string::size_type(0);
		while((pos = msg.find("\n", prev_pos)) != std::string::npos) {
			auto len = pos - prev_pos + 1;
			if(len > 0)
				_messages.emplace_back(record.getSeverity(),
				                       prev_pos == 0 ? msg.substr(prev_pos, pos - prev_pos)
				                                     : "    " + msg.substr(prev_pos, pos - prev_pos));

			prev_pos = pos + 1;
		}
	}


	Debug_ui::Debug_ui(asset::Asset_manager& assets, Gui& gui, util::Message_bus& bus)
	  : _gui(gui), _mailbox(bus), _assets(assets)
	{
		assets.open(history_aid).process([&](auto& is) { _history = is.lines(); });

		_commands.add("help | Prints all available commands", [&]() {
			auto stream    = std::stringstream{};
			auto max_width = 0;
			for(auto& c : util::Console_command_container::list_all_commands()) {
				auto sep  = c.second->api().find("|");
				max_width = std::max(max_width, int(sep != std::string::npos ? sep : c.second->api().size()));
			}

			for(auto& c : util::Console_command_container::list_all_commands()) {
				auto sep = c.second->api().find("|");

				stream << c.second->api().substr(0, sep);
				for(int i = int(sep != std::string::npos ? sep : c.second->api().size()); i < max_width + 10;
				    i++)
					stream << ' ';

				if(sep != std::string::npos)
					stream << c.second->api().substr(sep + 1);

				stream << "\n";
			}

			LOG(plog::info) << "Available commands:\n" << stream.str();
		});

		_commands.add("history | Prints all previous commands", [&]() {
			IF_PLOG_(PLOG_DEFAULT_INSTANCE_ID, plog::info)
			{
				auto record = plog::Record(plog::info,
				                           PLOG_GET_FUNC(),
				                           __LINE__,
				                           PLOG_GET_FILE(),
				                           PLOG_GET_THIS(),
				                           PLOG_DEFAULT_INSTANCE_ID);

				record.ref() << "History:\n";
				for(auto& h : _history) {
					record.ref() << h << "\n";
				}

				(*plog::get<PLOG_DEFAULT_INSTANCE_ID>()) += record.ref();
			}
		});
		_commands.add("history.clear | Clears the history", [&]() {
			_history.clear();
			_save_history();
		});

		_commands.add("show <ui> | Enables a specific debug UI element (see list_uis for possible options)",
		              [&](std::string ui) {
			              if(Debug_menu::is_debug_menu(ui)) {
				              _shown_debug_menus.insert(std::move(ui));
				              _show_console = false;
			              } else
				              LOG(plog::error) << "Unknown ui menu " << ui
				                               << " expected one of: " << Debug_menu::print_names();
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
				                               << " expected one of: " << Debug_menu::print_names();
		              });
		_commands.add("hide.all | Disables all debug UI elements", [&] { _shown_debug_menus.clear(); });

		_commands.add("list_uis | Lists all available debug UI elements",
		              [&]() { LOG(plog::info) << "UI menus: " << Debug_menu::print_names(); });

		_mailbox.subscribe_to([&](input::Once_action& e) {
			switch(e.id) {
				case "fast_quit"_strid: mirrage_quick_exit(); break;
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

		auto viewport = _gui.viewport();
		auto width    = viewport.z;
		auto height   = 300.f;

		ImGui::PositionNextWindow(
		        {width, height}, ImGui::WindowPosition_X::center, ImGui::WindowPosition_Y::top);
		if(ImGui::Begin("debug_console",
		                nullptr,
		                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
		                        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

			_text_filter.Draw("Filter (\"incl,-excl\") (\"error\")");
			ImGui::Separator();

			auto footer_height_to_reserve =
			        ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
			if(ImGui::BeginChild("ScrollingRegion",
			                     ImVec2(0, -footer_height_to_reserve),
			                     false,
			                     ImGuiWindowFlags_HorizontalScrollbar)) {

				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
				for(const auto& msg : debug_console_appender()._messages) {
					if(!_text_filter.PassFilter(msg.msg.c_str()))
						continue;

					ImGui::PushStyleColor(ImGuiCol_Text, msg_color[std::size_t(msg.severity)]);
					ImGui::TextUnformatted(msg.msg.c_str());
					ImGui::PopStyleColor();
				}

				if(_scroll_to_bottom) {
					ImGui::SetScrollHereY(1.0f);
					_scroll_to_bottom = false;
				}
				ImGui::PopStyleVar();
			}
			ImGui::EndChild();
			ImGui::Separator();


			// command prompt
			bool reclaim_focus = false;

			auto input_callback = +[](ImGuiInputTextCallbackData* data) -> int {
				auto self = reinterpret_cast<Debug_ui*>(data->UserData);
				switch(data->EventFlag) {
					case ImGuiInputTextFlags_CallbackCompletion: {
						auto cmd         = std::string(data->Buf, data->Buf + data->CursorPos);
						auto suggestions = util::Console_command_container::complete(cmd);

						if(suggestions.empty()) {
							debug_console_appender()._messages.emplace_back(plog::Severity::verbose,
							                                                "Unknown command: " + cmd);

						} else if(suggestions.size() == 1) {
							data->DeleteChars(0, int(cmd.size()));
							data->InsertChars(0, suggestions.front()->name().c_str());
							data->CursorPos = int(suggestions.front()->name().size());
							debug_console_appender()._messages.emplace_back(plog::Severity::verbose,
							                                                suggestions.front()->api());
						} else {
							// TODO: move to popup
							debug_console_appender()._messages.emplace_back(plog::Severity::verbose,
							                                                "Command Suggestions:");
							for(auto& s : suggestions) {
								debug_console_appender()._messages.emplace_back(plog::Severity::verbose,
								                                                s->api());
							}
						}
						self->_scroll_to_bottom = true;

						break;
					}

					case ImGuiInputTextFlags_CallbackHistory: {
						auto prev = self->_current_history_entry;
						if(data->EventKey == ImGuiKey_UpArrow) {
							if(self->_current_history_entry == -1)
								self->_current_history_entry = int(self->_history.size()) - 1;
							else if(self->_current_history_entry > 0)
								self->_current_history_entry--;
						} else if(data->EventKey == ImGuiKey_DownArrow) {
							if(self->_current_history_entry != -1)
								if(++self->_current_history_entry >= int(self->_history.size()))
									self->_current_history_entry = -1;
						}

						// A better implementation would preserve the data on the current input line along with cursor position.
						if(prev != self->_current_history_entry) {
							const char* history_str =
							        (self->_current_history_entry >= 0)
							                ? self->_history[std::size_t(self->_current_history_entry)].c_str()
							                : "";
							data->DeleteChars(0, data->BufTextLen);
							data->InsertChars(0, history_str);
						}
						break;
					}
				}
				return 0;
			};

			if(ImGui::InputText("",
			                    &_command,
			                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion
			                            | ImGuiInputTextFlags_CallbackHistory,
			                    input_callback,
			                    this)) {
				if(util::Console_command_container::call(_command)) {
					_history.emplace_back(_command);
					_save_history();
					_command.clear();
				}
				reclaim_focus = true;
			}

			// Auto-focus on window apparition
			ImGui::SetItemDefaultFocus();
			if(reclaim_focus)
				ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
		}
		ImGui::End();
	}

	void Debug_ui::_save_history()
	{
		auto os = _assets.open_rw(history_aid);
		for(auto& h : _history)
			os << h << "\n";
		os.close();
	}


	Debug_menu::Debug_menu(std::string name) : _name(std::move(name)) { instances().emplace_back(this); }
	Debug_menu::~Debug_menu() { util::erase_fast(instances(), this); }

	void Debug_menu::draw_all(const std::string& name, Gui& gui)
	{
		for(auto dm : instances())
			if(dm->_name == name)
				dm->draw(gui);
	}
	auto Debug_menu::is_debug_menu(const std::string& name) -> bool
	{
		for(auto dm : instances())
			if(dm->_name == name)
				return true;

		return false;
	}
	auto Debug_menu::print_names() -> std::string
	{
		auto stream = std::stringstream{};
		auto first  = true;
		for(auto dm : instances()) {
			if(first)
				first = false;
			else
				stream << ", ";

			stream << dm->_name;
		}
		return stream.str();
	}

} // namespace mirrage::gui
