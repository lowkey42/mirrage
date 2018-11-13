/** manages the screens of the application (game, menues, etc.) **************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/asset/asset_manager.hpp>

#include <mirrage/utils/defer.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/ranges.hpp>
#include <mirrage/utils/units.hpp>

#include <memory>
#include <vector>


namespace mirrage {
	class Engine;

	enum class Prev_screen_policy { discard, stack, draw, update };

	class Screen : private util::Deferred_action_container {
	  public:
		Screen(Engine& engine) : _engine(engine) {}
		Screen(const Screen&) = delete;
		Screen& operator=(const Screen&) = delete;

		virtual ~Screen() noexcept;

		using Deferred_action_container::defer;

		virtual auto name() const -> std::string = 0;

	  protected:
		friend class Screen_manager;

		virtual void _on_enter(util::maybe<Screen&> prev) {}
		virtual void _on_leave(util::maybe<Screen&> next) {}
		virtual void _update(util::Time delta_time)                             = 0;
		virtual void _draw()                                                    = 0;
		virtual auto _prev_screen_policy() const noexcept -> Prev_screen_policy = 0;

		Engine& _engine;

	  private:
		void _actual_update(util::Time delta_time);
	};

	class Screen_manager {
		friend class Screen;

	  public:
		Screen_manager(Engine& engine);
		~Screen_manager() noexcept;

		template <class T, typename... Args>
		auto enter(Args&&... args) -> T&
		{
			return static_cast<T&>(enter(std::make_unique<T>(_engine, std::forward<Args>(args)...)));
		}

		auto enter(std::unique_ptr<Screen> screen) -> Screen&;
		void leave(uint8_t depth = 1);
		auto current() -> Screen&;

		void on_frame(util::Time delta_time);
		void do_queued_actions();
		void clear();

		template <class Stream>
		auto print_stack(Stream& out) const -> auto&
		{
			for(auto& screen : util::range_reverse(_screen_stack)) {
				out << screen->name();
				switch(screen->_prev_screen_policy()) {
					case Prev_screen_policy::discard: out << " |"; break;
					case Prev_screen_policy::stack: out << " S> "; break;
					case Prev_screen_policy::draw: out << " D> "; break;
					case Prev_screen_policy::update: out << " U> "; break;
				}
			}

			return out;
		}

	  protected:
		Engine&                              _engine;
		std::vector<std::shared_ptr<Screen>> _screen_stack;
		int32_t                              _leave = 0;
		std::vector<std::shared_ptr<Screen>> _next_screens;
	};
} // namespace mirrage
