#include <mirrage/utils/console_command.hpp>

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/ranges.hpp>


namespace mirrage::util {
	Console_command_container::~Console_command_container()
	{
		auto& cmds = all_commands();
		for(auto iter = cmds.begin(); iter != cmds.end();) {
			if(std::find(_command_ids.begin(), _command_ids.end(), iter->second.id()) != _command_ids.end()) {
				iter = cmds.erase(iter);
			} else {
				iter++;
			}
		}
	}
	auto Console_command_container::call(std::string_view cmd) -> bool
	{
		auto arg_sep      = cmd.find(" ");
		auto cmd_name     = std::string(cmd.substr(0, arg_sep));
		auto [begin, end] = all_commands().equal_range(cmd_name);

		if(begin == end) {
			LOG(plog::error) << "Unknown console command: " << cmd_name;
			return false;
		}

		auto args = cmd.substr(arg_sep);

		for(auto& [key, value] : util::range(begin, end))
			value.call(args);

		return true;
	}

	auto Console_command_container::complete(std::string_view input) -> std::vector<Console_command*>
	{
		auto cmd = input.substr(0, input.find(" "));

		auto matches = std::vector<Console_command*>();
		for(auto&& [key, value] : all_commands()) {
			if(cmd.size() <= key.size() && std::string_view(key.c_str(), cmd.size()) == cmd) {
				matches.emplace_back(&value);
			}
		}

		return matches;
	}
} // namespace mirrage::util
