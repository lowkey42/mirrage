/** math helpers *************************************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/units.hpp>

#include <tuple>
#include <memory>
#include <vector>


namespace mirrage {
namespace util {

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
