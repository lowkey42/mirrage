/** implementation of command pattern (e.g. for undo/redo) *******************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/log.hpp>

#include <memory>
#include <string>
#include <vector>


namespace mirrage::util {

	struct Command {
		Command()          = default;
		virtual ~Command() = default;

		virtual void execute()                          = 0;
		virtual void undo()                             = 0;
		virtual auto name() const -> const std::string& = 0;
	};

	using Command_marker = const void*;

	class Command_manager {
	  public:
		void execute(std::unique_ptr<Command> cmd);

		template <class T, class... Args>
		T& execute(Args&&... args) {
			auto  ptr = std::make_unique<T>(std::forward<Args>(args)...);
			auto& ref = *ptr;
			execute(std::move(ptr));

			return ref;
		}

		void clear() {
			_commands.clear();
			_history_size = 0;
		}

		void undo();
		void redo();

		auto history() const -> std::vector<std::string>;
		auto future() const -> std::vector<std::string>;

		auto undo_available() const -> bool { return _history_size > 0; }
		auto redo_available() const -> bool { return _history_size < _commands.size(); }

		bool is_last(Command_marker marker) const;
		auto get_last() const -> Command_marker;

	  private:
		std::vector<std::unique_ptr<Command>> _commands;
		std::size_t                           _history_size = 0;
	};
} // namespace mirrage::util
