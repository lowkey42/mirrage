#pragma once

#include <indicators/block_progress_bar.hpp>
#include <indicators/multi_progress.hpp>

#include <plog/Util.h>

#include <memory>


namespace mirrage::helper {

	using Progress_bar = indicators::BlockProgressBar;

	class Progress_container;

	class Progress_ref {
	  public:
		Progress_ref() : _container(nullptr), _index(0) {}

		void progress(float p);
		void color(indicators::Color color);
		void message(const std::string& message);

	  private:
		friend class Progress_container;
		Progress_container* _container;
		std::size_t         _index;

		Progress_ref(Progress_container& container, std::size_t index) : _container(&container), _index(index)
		{
		}
	};

	class Progress_container {
	  public:
		Progress_container(plog::util::Mutex& mutex) : _mutex(mutex) {}

		auto add(indicators::Color color, const std::string& message) -> Progress_ref;

		void restart();

		void enable() { _enabled = true; }

	  private:
		friend class Progress_ref;

		struct Element {
			std::unique_ptr<Progress_bar> bar;
			float                         progress      = 0.f;
			float                         last_progress = 0.f;
		};

		std::atomic<bool>    _enabled{false};
		bool                 _started{false};
		plog::util::Mutex&   _mutex;
		std::vector<Element> _bars;

		void _print(); //< _mutex must be locked, when called
		void _bar_progress(std::size_t, float p);
		void _bar_color(std::size_t, indicators::Color color);
		void _bar_message(std::size_t, const std::string& msg);
	};

} // namespace mirrage::helper
