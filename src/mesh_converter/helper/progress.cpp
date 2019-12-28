#include "progress.hpp"

namespace mirrage::helper {

	void Progress_ref::progress(float p)
	{
		if(_container)
			_container->_bar_progress(_index, p);
	}
	void Progress_ref::color(indicators::Color color)
	{
		if(_container)
			_container->_bar_color(_index, color);
	}
	void Progress_ref::message(const std::string& message)
	{
		if(_container)
			_container->_bar_message(_index, message);
	}

	auto Progress_container::add(indicators::Color color, const std::string& message) -> Progress_ref
	{
		auto p = std::make_unique<Progress_bar>();
		p->show_percentage();
		p->set_foreground_color(color);
		p->set_postfix_text(message);
		p->set_bar_width(60);

		auto lock = plog::util::MutexLock{_mutex};

		auto idx = _bars.size();
		_bars.emplace_back(Element{std::move(p), 0.f, 0.f});
		_print();
		return Progress_ref{*this, idx};
	}

	void Progress_container::restart() { _started = false; }

	void Progress_container::_bar_progress(std::size_t idx, float p)
	{
		auto  lock   = plog::util::MutexLock{_mutex};
		auto& bar    = _bars[idx];
		bar.progress = p;
		if(p - bar.last_progress > 0.002f)
			_print();
	}
	void Progress_container::_bar_color(std::size_t idx, indicators::Color color)
	{
		auto lock = plog::util::MutexLock{_mutex};
		_bars[idx].bar->set_foreground_color(color);
		_print();
	}
	void Progress_container::_bar_message(std::size_t idx, const std::string& msg)
	{
		auto lock = plog::util::MutexLock{_mutex};
		_bars[idx].bar->set_postfix_text(msg);
		_print();
	}

	void Progress_container::_print()
	{
		if(_bars.empty() || !_enabled)
			return;

		if(_started) {
			std::cout << "\x1b[A";
			for(auto& _ : _bars) {
				(void) _;
				std::cout << "\x1b[A";
			}
		}

		std::cout << "\n";
		for(auto& [bar, progress, last_progress] : _bars) {
			last_progress = progress;
			bar->set_progress(std::min(100.f, progress * 100.f));
			std::cout << termcolor::reset << "\n";
		}
		std::cout << std::flush;
		_started = true;
	}

} // namespace mirrage::helper
