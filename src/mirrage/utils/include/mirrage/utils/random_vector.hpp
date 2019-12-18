#pragma once

#include <algorithm>
#include <random>
#include <vector>


namespace mirrage::util {

	template <class T>
	class random_vector {
	  public:
		random_vector(std::vector<T> data = {}) : _data(std::move(data)) {}

		auto raw_vector() -> std::vector<T>& { return _data; }

		auto empty() const { return _data.empty(); }

		template <class RNG>
		auto get_random(RNG& gen) -> T&
		{
			if(_data.size() <= 1)
				return _data.front();

			if(_next_index < _data.size())
				return _data[_next_index++];

			if(_data.size() > 2) {
				auto low  = static_cast<std::size_t>(std::ceil(_data.size() / 4.f));
				auto high = static_cast<std::size_t>(std::floor(_data.size() / 4.f * 3.f));
				std::shuffle(_data.begin(), _data.begin() + high, gen);
				std::shuffle(_data.begin() + low, _data.end(), gen);
			}

			_next_index = 1;
			return _data[0];
		}

	  private:
		std::vector<T> _data;
		std::size_t    _next_index = 0;
	};

} // namespace mirrage::util
