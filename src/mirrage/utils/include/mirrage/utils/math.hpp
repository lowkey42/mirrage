/** math helpers *************************************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/units.hpp>

#include <tuple>
#include <type_traits>
#include <memory>
#include <vector>


namespace mirrage {
namespace util {

	namespace details {
		template<typename... Ts>
	    struct min_max_result {
			using common_type = std::common_type_t<Ts...>;
			using no_temporaries = std::conjunction<std::negation<std::is_lvalue_reference<Ts>>...>;
			using reference_allowed = std::conjunction<std::is_convertible<std::add_lvalue_reference_t<Ts>,
			                                                               std::add_lvalue_reference_t<common_type>>...>;

			using use_references = std::conjunction<no_temporaries, reference_allowed>;

			using type = std::conditional_t<use_references::value, std::add_lvalue_reference_t<common_type>, common_type>;
	    };

			template<typename... Ts>
			using min_max_result_t = typename min_max_result<Ts...>::type;
	}

	// min
	template<typename FirstT>
	constexpr auto min(FirstT&& first) {
		return first;
	}

	template<typename FirstT, typename SecondT>
	constexpr auto min(FirstT&& first, SecondT&& second) -> details::min_max_result_t<FirstT, SecondT> {
		if(first < second)
			return first;
		else
			return second;
	}

	template<typename FirstT, typename SecondT, typename... Ts>
	constexpr auto min(FirstT&& first, SecondT&& second, Ts&&... rest) -> details::min_max_result_t<FirstT, SecondT, Ts...> {
		return min(min(std::forward<FirstT>(first), std::forward<SecondT>(second)),
		           std::forward<Ts>(rest)...);
	}

	// max
	template<typename FirstT>
	constexpr auto max(FirstT&& first) {
		return first;
	}

	template<typename FirstT, typename SecondT>
	constexpr auto max(FirstT&& first, SecondT&& second) {
		using result_t = details::min_max_result_t<FirstT, SecondT>;

		if(static_cast<result_t>(second) < static_cast<result_t>(first))
			return static_cast<result_t>(first);
		else
			return static_cast<result_t>(second);
	}

	template<typename FirstT, typename SecondT, typename... Ts>
	constexpr auto max(FirstT&& first, SecondT&& second, Ts&&... rest) -> details::min_max_result_t<FirstT, SecondT, Ts...> {
		return max(max(std::forward<FirstT>(first), std::forward<SecondT>(second)),
		           std::forward<Ts>(rest)...);
	}


	template<typename Pos, typename Vel>
	auto spring(Pos source, Vel v, Pos target, float damping,
	            float freq, Time t) -> std::tuple<Pos, Vel> {
		auto f = remove_unit(1 + 2*t*damping*freq);
		auto tff = remove_unit(t*freq*freq);
		auto ttff = remove_unit(t*tff);
		auto detInv = 1.f / (f+ttff);
		auto diff = remove_units(target-source);

		auto new_pos = (f * source + t*v+ttff*target) * detInv;
		auto new_vel = (v + tff * Vel{diff.x, diff.y}) * detInv;

		if((remove_unit(new_vel.x)*remove_unit(new_vel.x) + remove_unit(new_vel.y)*remove_unit(new_vel.y))<0.5f)
			new_vel = new_vel * 0.f;

		return std::make_tuple(new_pos, new_vel);
	}


	enum class Interpolation_type {
		linear,
		constant
	};

	template<typename T>
	struct Interpolation {
		Interpolation(T begin, T end, Interpolation_type type,
					  T max_deviation, std::vector<T> cpoints)noexcept;

		auto operator()(float t, int32_t seed=0)const noexcept -> T;

		auto max()const noexcept -> T;
		auto avg(int32_t seed=0)const noexcept -> T;

		T                  initial_value;
		T                  final_value;
		Interpolation_type type;
		T                  max_deviation;
		std::vector<T>     cpoints;
	};

	template<typename T>
	using Xerp = Interpolation<T>;

	/**
	 * linear interpolation
	 */
	template<typename T>
	auto lerp(T begin, T end, T max_deviation = T(0)) -> Interpolation<T>;

	/**
	 * constant 'interpolation'
	 */
	template<typename T>
	auto cerp(std::vector<T> values, T max_deviation = T(0)) -> Interpolation<T>;

	/**
	 * single constant 'interpolation'
	 */
	template<typename T>
	auto scerp(T value, T max_deviation = T(0)) -> Interpolation<T>;


	template<typename T>
	auto operator*(Interpolation<T> i, float f) -> Interpolation<T> {
		auto cpoints = std::vector<T>(i.cpoints);
		for(auto& p : cpoints) p*=f;

		return {
			i.initial_value * f,
			i.final_value * f,
			i.type,
			i.max_deviation * f,
			std::move(cpoints)
		};
	}
	template<typename T>
	auto operator/(Interpolation<T> i, float f) -> Interpolation<T> {
		return i * (1/f);
	}
}
}

#define UTIL_MATH_INCLUDED
#include "math.hxx"
