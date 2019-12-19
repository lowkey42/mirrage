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

	namespace detail {
		template <typename T>
		struct Default_var_setter {
			void operator()(T& org, T new_val) { org = new_val; }
		};
	} // namespace detail

	class Console_command_container {
	  public:
		~Console_command_container();

		template <typename F>
		auto add(std::string api, F &&) -> Console_command_container&;

		template <typename T, typename F = detail::Default_var_setter<T>>
		auto add_var(const std::string& name, T& var, F setter = {})
		{
			add_property(
			        name,
			        [&var, setter = std::move(setter)](T new_val) mutable { setter(var, new_val); },
			        [&var, name = name]() -> decltype(auto) { return var; });
		}

		template <typename FS, typename FG>
		auto add_property(const std::string& name, FS&& setter, FG&& getter)
		{
			add("set." + name + " <value> | Sets the value of the property", std::forward<FS>(setter));
			add("get." + name + " | Gets the value of the property",
			    [name = name, getter = std::forward<FG>(getter)] {
				    LOG(plog::info) << "Value of " << name << ": " << util::to_string(getter());
			    });
		}

		/// returns false if no such command could be found
		static auto call(std::string_view cmd) -> bool;

		static auto complete(std::string_view input) -> std::vector<Console_command*>;

		static auto list_all_commands() -> const auto& { return all_commands(); }

	  private:
		static auto all_commands() -> std::unordered_multimap<std::string, std::unique_ptr<Console_command>>&
		{
			static std::unordered_multimap<std::string, std::unique_ptr<Console_command>> cmds;
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
		        std::regex(R"xxx(\"((?:[^"]|\\")+(?!\\).)\"|([^ ]+))xxx", std::regex::optimize);

		auto name = api.substr(0, api.find(" "));

		auto cmd_callback = [api, f = std::forward<F>(f)](std::string_view cmd) mutable {
			auto arg_iter = std::cregex_iterator(cmd.data(), cmd.data() + cmd.size(), split_args_regex);
			auto arg_end  = std::cregex_iterator();

			util::foreach_function_arg_call(
			        f, [&](auto type) -> util::maybe<typename decltype(type)::type> {
				        if(arg_iter == arg_end) {
					        LOG(plog::error) << "Not enough arguments.";
					        return util::nothing;
				        }

				        auto curr = *arg_iter;
				        arg_iter++;

				        if(curr[1].length() == 0) {
					        return util::from_string<typename decltype(type)::type>(
					                cmd.substr(std::size_t(curr.position()), std::size_t(curr.length())));

				        } else { // quoted
					        auto arg = std::string_view(curr[1].first, std::size_t(curr[1].length()));
					        if(!arg.find("\\"))
						        return util::from_string<typename decltype(type)::type>(arg);
					        else {
						        // contains escape sequences we have to replace
						        auto arg_str = std::string(arg);
						        util::replace_inplace(arg_str, "\\\"", "\"");
						        util::replace_inplace(arg_str, "\\\\", "\\");
						        return util::from_string<typename decltype(type)::type>(arg_str);
					        }
				        }
			        });
		};

		auto c = all_commands().emplace(std::move(name),
		                                std::make_unique<Console_command>(name, api, cmd_callback));

		_command_ids.emplace_back(c->second->id());
		return *this;
	}

} // namespace mirrage::util
