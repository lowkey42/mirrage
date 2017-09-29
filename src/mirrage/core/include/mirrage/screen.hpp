/** manages the screens of the application (game, menues, etc.) **************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/units.hpp>

#include <memory>
#include <vector>


namespace mirrage {
	class Engine;

	enum class Prev_screen_policy { discard, stack, draw, update };

	class Screen {
	  public:
		Screen(Engine& engine) : _engine(engine) {}
		Screen(const Screen&) = delete;
		Screen& operator=(const Screen&) = delete;

		virtual ~Screen() noexcept = default;

	  protected:
		friend class Screen_manager;

		virtual void _on_enter(util::maybe<Screen&> prev) {}
		virtual void _on_leave(util::maybe<Screen&> next) {}
		virtual void _update(util::Time delta_time)                             = 0;
		virtual void _draw()                                                    = 0;
		virtual auto _prev_screen_policy() const noexcept -> Prev_screen_policy = 0;

		Engine& _engine;
	};

	class Screen_manager {
		friend class Screen;

	  public:
		Screen_manager(Engine& engine);
		~Screen_manager() noexcept;

		template <class T, typename... Args>
		auto enter(Args&&... args) -> T& {
			return static_cast<T&>(enter(std::make_unique<T>(_engine, std::forward<Args>(args)...)));
		}

		auto enter(std::unique_ptr<Screen> screen) -> Screen&;
		void leave(uint8_t depth = 1);
		auto current() -> Screen&;

		void on_frame(util::Time delta_time);
		void do_queued_actions();
		void clear();

	  protected:
		Engine&                              _engine;
		std::vector<std::shared_ptr<Screen>> _screen_stack;
		int32_t                              _leave = 0;
		std::vector<std::shared_ptr<Screen>> _next_screens;
	};
} // namespace mirrage
