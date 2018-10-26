#include <mirrage/utils/defer.hpp>


namespace mirrage::util {

	void Deferred_action_container::defer(util::Time time, std::function<void()> callback)
	{
		_defered_actions.emplace_back(
		        [t = _time_since_start + time.value(), this] { return t <= _time_since_start; }, callback);
	}

	void Deferred_action_container::update(util::Time delta_time)
	{
		_time_since_start += delta_time.value();

		auto new_end = std::partition(
		        _defered_actions.begin(), _defered_actions.end(), [](auto& a) { return !a.ready(); });

		if(new_end != _defered_actions.end()) {
			auto saved = std::vector(new_end, _defered_actions.end());
			_defered_actions.erase(new_end, _defered_actions.end());

			for(auto& a : saved) {
				a.callback();
			}
		}
	}

} // namespace mirrage::util
