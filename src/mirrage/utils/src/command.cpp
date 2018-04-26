#include <mirrage/utils/command.hpp>

#include <mirrage/utils/log.hpp>


namespace mirrage::util {

	Command::~Command() = default;

	void Command_manager::execute(std::unique_ptr<Command> cmd) {
		if(redo_available())
			_commands.resize(_history_size);

		cmd->execute();
		_commands.emplace_back(std::move(cmd));
		_history_size = _commands.size();
	}
	void Command_manager::undo() {
		MIRRAGE_INVARIANT(undo_available(), "undo() called with empty history");

		auto& last_cmd = _commands.at(_history_size - 1);
		last_cmd->undo();
		_history_size--;
	}
	void Command_manager::redo() {
		MIRRAGE_INVARIANT(redo_available(), "redo() called with empty future");

		auto& next_cmd = _commands.at(_history_size);
		next_cmd->execute();
		_history_size++;
	}

	auto Command_manager::history() const -> std::vector<std::string> {
		std::vector<std::string> names;
		names.reserve(_history_size);

		for(auto i = 0ul; i < _history_size; i++) {
			names.emplace_back(_commands.at(i)->name());
		}

		return names;
	}
	auto Command_manager::future() const -> std::vector<std::string> {
		std::vector<std::string> names;
		names.reserve(_commands.size() - _history_size);

		for(auto i = _history_size; i < _commands.size(); i++) {
			names.emplace_back(_commands.at(i)->name());
		}

		return names;
	}

	bool Command_manager::is_last(Command_marker marker) const {
		return !_commands.empty() && !redo_available() && _commands.back().get() == marker;
	}
	auto Command_manager::get_last() const -> Command_marker {
		if(_commands.empty() || _history_size <= 0)
			return nullptr;
		else
			return _commands.at(_history_size - 1).get();
	}
} // namespace mirrage::util
