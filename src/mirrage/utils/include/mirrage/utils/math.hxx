#ifndef UTIL_MATH_INCLUDED
#include "math.hpp"
#endif


namespace mirrage::util {

	namespace {
		constexpr int32_t small_magic_prime = 337;
		constexpr int32_t magic_prime       = 1'299'827;
		constexpr auto    max_seed          = 1024;
		constexpr auto    max_seed_real     = 1024.f;
	}

	template <typename T>
	Interpolation<T>::Interpolation(
	        T begin, T end, Interpolation_type type, T max_deviation, std::vector<T> cpoints) noexcept
	  : initial_value(begin), final_value(end), type(type), max_deviation(max_deviation), cpoints(cpoints) {}

	template <typename T>
	auto Interpolation<T>::operator()(float t, int32_t seed) const noexcept -> T {
		int32_t base = (seed * small_magic_prime) % max_seed;
		T       val  = (base / max_seed_real) * max_deviation * 2.f - max_deviation;

		switch(type) {
			case Interpolation_type::linear: val += (1 - t) * initial_value + t * final_value; break;

			case Interpolation_type::constant:
				val += cpoints.at((seed + magic_prime) % cpoints.size());
				break;
		}

		return val;
	}

	template <typename T>
	auto Interpolation<T>::avg(int32_t seed) const noexcept -> T {
		auto& self = *this;

		return (self(0, seed) + self(1, seed)) / 2.f;
	}

	template <typename T>
	auto Interpolation<T>::max() const noexcept -> T {
		using std::max;
		return max(initial_value, final_value) + max_deviation;
	}

	template <typename T>
	auto lerp(T begin, T end, T max_deviation) -> Interpolation<T> {
		return {begin, end, Interpolation_type::linear, max_deviation, {}};
	}

	template <typename T>
	auto cerp(std::vector<T> values, T max_deviation) -> Interpolation<T> {
		using std::max;

		T max_v{0};
		for(const auto& v : values)
			max_v = max(max_v, v);

		return {T{0}, T{max_v}, Interpolation_type::constant, max_deviation, std::move(values)};
	}

	template <typename T>
	auto scerp(T value, T max_deviation) -> Interpolation<T> {
		return {value, value, Interpolation_type::linear, max_deviation, {}};
	}
}
