/** container to defer action until later without multithreading *************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/units.hpp>

#include <functional>
#include <future>
#include <vector>


namespace mirrage::util {

	class Deferred_action_container {
	  public:
		void defer(util::Time, std::function<void()> callback);
		template <class Future, class F, typename = std::enable_if_t<!std::is_same_v<Future, util::Time>>>
		void defer(Future f, F&& callback)
		{
			_defered_actions.emplace_back([f] { return f.ready(); },
			                              [f, callback = std::move(callback)] { callback(*f); });
		}
		template <class T, class F>
		void defer(std::future<T> f, F&& callback)
		{
			auto sf = f.share();
			_defered_actions.emplace_back(
			        [sf] { return sf.wait_for(std::chrono::seconds(0)) == std::future_status::ready; },
			        [sf, callback = std::move(callback)] { callback(sf.get()); });
		}

		void update(util::Time dt);

	  private:
		struct Defered_action {
			std::function<bool()> ready;
			std::function<void()> callback;

			Defered_action(std::function<bool()> ready, std::function<void()> callback)
			  : ready(std::move(ready)), callback(std::move(callback))
			{
			}
		};

		float                       _time_since_start = 0;
		std::vector<Defered_action> _defered_actions;
	};

} // namespace mirrage::util
