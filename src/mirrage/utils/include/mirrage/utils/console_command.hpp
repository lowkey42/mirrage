/** global registry for CLI commands *****************************************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/func_traits.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/string_utils.hpp>

#include <functional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>


namespace mirrage::util {

	/*
	 * auto commands = Console_command_container{};
	 * commands.add("show <window>", [&](auto& window) {
	 *	// stuff
	 * });
	 */

	class Console_command {
	  public:
		using Func = std::function<void(std::string_view)>;

		Console_command(std::string name, std::string api, Func exec)
		  : _id(gen_id()), _name(std::move(name)), _api(std::move(api)), _exec(std::move(exec))
		{
		}

		auto id() const noexcept { return _id; }
		auto name() const noexcept -> auto& { return _name; }
		auto api() const noexcept -> auto& { return _api; }
		void call(std::string_view args) const { _exec(args); }

	  private:
		static auto gen_id() -> std::int64_t
		{
			static auto next = std::int64_t(0);
			return next++;
		}

		std::int64_t _id;
		std::string  _name;
		std::string  _api;
		Func         _exec;
	};

	class Console_command_container {
	  public:
		~Console_command_container();

		template <typename F>
		auto add(std::string api, F &&) -> Console_command_container&;

		/// returns false if no such command could be found
		static auto call(std::string_view cmd) -> bool;

		static auto complete(std::string_view input) -> std::vector<Console_command*>;

	  private:
		static auto all_commands() -> auto&
		{
			static std::unordered_multimap<std::string, Console_command> cmds;
			return cmds;
		}

		std::vector<std::int64_t> _command_ids;
	};


	namespace detail {
		template <typename T>
		struct Type_wrapper {
		};
	} // namespace detail

	template <typename F>
	auto Console_command_container::add(std::string api, F&& f) -> Console_command_container&
	{
		static const auto split_args_regex =
		        std::regex(R"xxx(\"((?:[^"]|\\")+)(?!\\).\"|([^ ]+))xxx", std::regex::optimize);

		auto name = api.substr(0, api.find(" "));
		auto c    = all_commands().emplace(
                std::piecewise_construct,
                std::forward_as_tuple(name),
                std::forward_as_tuple(name, api, [api, f = std::forward<F>(f)](std::string_view cmd) {
                    auto arg_iter = std::cregex_iterator(cmd.begin(), cmd.end(), split_args_regex);
                    auto arg_end  = std::cregex_iterator();

                    util::foreach_function_arg_call(
                            f, [&](auto type) -> util::maybe<typename decltype(type)::type> {
                                if(arg_iter == arg_end) {
                                    LOG(plog::error) << "Not enough arguments.";
                                    return util::nothing;
                                } else {
                                    // TODO: unescape quotes if the argument was quoted!
                                    auto arg =
                                            (*arg_iter)[1].length() == 0
                                                    ? cmd.substr(std::size_t(arg_iter->position()),
                                                                 std::size_t(arg_iter->length()))
                                                    : std::string_view((*arg_iter)[1].first,
                                                                       std::size_t((*arg_iter)[1].length()));
                                    return util::from_string<typename decltype(type)::type>(arg);
                                }
                            });
                }));

		_command_ids.emplace_back(c->second.id());
		return *this;
	}

} // namespace mirrage::util
