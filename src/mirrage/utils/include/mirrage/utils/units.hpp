/** Types and literal-operators for physical units ***************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <glm/gtc/constants.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/vec2.hpp>

#include <cmath>
#include <type_traits>


namespace mirrage::util {

	constexpr float PI = 3.14159265358979323846264338327950288f;

	inline constexpr float clamp(float v, float min, float max) noexcept
	{
		return v > min ? (v > max ? max : v) : min;
	}

	using Rgba = glm::vec4;
	using Rgb  = glm::vec3;

	template <class S>
	struct Value_type {
	  protected:
		constexpr Value_type(float v = 0) noexcept : val(v) {}

		float val;

	  public:
		constexpr float value() const noexcept { return val; }

		S operator++() noexcept
		{
			val++;
			return static_cast<S&>(*this);
		}
		S operator--() noexcept
		{
			val--;
			return static_cast<S&>(*this);
		}

		S operator++(int) noexcept
		{
			S t(val);
			val++;
			return t;
		}
		S operator--(int) noexcept
		{
			S t(val);
			val--;
			return t;
		}

		S operator+=(S b) noexcept
		{
			val += b.val;
			return static_cast<S&>(*this);
		}
		S operator-=(S b) noexcept
		{
			val -= b.val;
			return static_cast<S&>(*this);
		}
		S operator*=(float v) noexcept
		{
			val *= v;
			return static_cast<S&>(*this);
		}
		S operator/=(float v) noexcept
		{
			val /= v;
			return static_cast<S&>(*this);
		}

		constexpr bool operator==(S b) const noexcept { return val == b.value(); }
		constexpr bool operator==(float v) const noexcept { return val == v; }
		constexpr bool operator!=(S b) const noexcept { return val != b.value(); }
		constexpr bool operator!=(float v) const noexcept { return val != v; }
		constexpr bool operator<=(S b) const noexcept { return val <= b.value(); }
		constexpr bool operator<(S b) const noexcept { return val < b.value(); }
		constexpr bool operator>=(S b) const noexcept { return val >= b.value(); }
		constexpr bool operator>(S b) const noexcept { return val > b.value(); }

		friend constexpr S operator-(S a) noexcept { return S(-a.value()); }
		friend constexpr S operator+(S a, S b) noexcept { return S(a.value() + b.value()); }
		friend constexpr S operator+(S a, float b) noexcept { return S(a.value() + b); }
		friend constexpr S operator+(float a, S b) noexcept { return S(a + b.value()); }
		friend constexpr S operator-(S a, S b) noexcept { return S(a.value() - b.value()); }
		friend constexpr S operator-(S a, float b) noexcept { return S(a.value() - b); }
		friend constexpr S operator-(float a, S b) noexcept { return S(a - b.value()); }
		friend constexpr S operator*(S a, float b) noexcept { return S(a.value() * b); }
		friend constexpr S operator*(float a, S b) noexcept { return S(a * b.value()); }
		friend constexpr S operator/(S a, float b) noexcept { return S(a.value() / b); }

		friend constexpr float operator/(S a, S b) noexcept { return a.value() / b.value(); }

		friend constexpr S abs(S v) noexcept { return v.value() < 0 ? -v : v; }
		friend constexpr S sign(S v) noexcept
		{
			return (v.value() < 0) ? S(-1) : ((v.value() > 0) ? S(1) : S(0));
		}
		friend constexpr S clamp(S v, S min, S max) noexcept
		{
			return S{clamp(v.value(), min.value(), max.value())};
		}
		friend constexpr S min(S v, S min) noexcept { return S{glm::min(v.value(), min.value())}; }
		friend constexpr S max(S v, S max) noexcept { return S{glm::max(v.value(), max.value())}; }
	};

	template <typename T>
	struct is_value_type : std::is_base_of<Value_type<T>, T> {
	};


	struct Distance : Value_type<Distance> {
		constexpr explicit Distance(float meters = 0) noexcept : Value_type(meters) {}
	};

	struct Angle : Value_type<Angle> {
		constexpr explicit Angle(float radians = 0) noexcept : Value_type(radians) {}
		constexpr             operator float() const noexcept { return val; }
		constexpr float       in_degrees() const noexcept { return val * (180.f / PI); }
		constexpr static auto from_degrees(float d) { return Angle(d / (180.f / PI)); }
	};

	inline float sin(Angle a) noexcept { return std::sin(a.value()); }
	inline float cos(Angle a) noexcept { return std::cos(a.value()); }
	inline float tan(Angle a) noexcept { return std::tan(a.value()); }

	inline constexpr Angle normalize(Angle a) noexcept
	{
		return Angle{((a.value() / (2.f * PI)) - int(a.value() / (2.f * PI))) * 2.f * PI};
	}


	struct Time : Value_type<Time> {
		constexpr explicit Time(float seconds = 0) noexcept : Value_type(seconds) {}
	};
	class Time_squared : public Value_type<Time_squared> {
		constexpr explicit Time_squared(float seconds = 0) noexcept : Value_type(seconds) {}

		friend constexpr Time_squared operator*(Time a, Time b) noexcept;
	};


	struct Force : Value_type<Force> {
		constexpr explicit Force(float newton = 0) noexcept : Value_type(newton) {}
	};

	struct Mass : Value_type<Mass> {
		constexpr explicit Mass(float g = 0) noexcept : Value_type(g) {}
	};
	struct Inv_mass : Value_type<Inv_mass> {
		constexpr explicit Inv_mass(Mass w) noexcept : Value_type(1.f / w.value()) {}
		constexpr explicit Inv_mass(float g = 0) noexcept : Value_type(g) {}
	};

	struct Speed : Value_type<Speed> {
		constexpr explicit Speed(float meterPs = 0) noexcept : Value_type(meterPs) {}
	};
	struct Speed_per_time : Value_type<Speed_per_time> {
		constexpr explicit Speed_per_time(float meter_pss = 0) noexcept : Value_type(meter_pss) {}
	};

	struct Angle_per_time : Value_type<Angle_per_time> {
		constexpr explicit Angle_per_time(float radians = 0) noexcept : Value_type(radians) {}
	};
	using Angle_velocity = Angle_per_time;
	struct Angle_acceleration : Value_type<Angle_acceleration> {
		constexpr explicit Angle_acceleration(float radians = 0) noexcept : Value_type(radians) {}
	};


	// directed units
	using Position     = glm::tvec3<Distance, glm::highp>;
	using Dir_force    = glm::tvec3<Force, glm::highp>;
	using Velocity     = glm::tvec3<Speed, glm::highp>;
	using Acceleration = glm::tvec3<Speed_per_time, glm::highp>;

	inline Position  operator/(Position a, float b) noexcept { return Position(a.x / b, a.y / b, a.z / b); }
	inline Position  operator*(Position a, float b) noexcept { return Position(a.x * b, a.y * b, a.z * b); }
	inline Position  operator*(float b, Position a) noexcept { return Position(a.x * b, a.y * b, a.z * b); }
	inline Dir_force operator*(Dir_force a, float b) noexcept { return Dir_force(a.x * b, a.y * b, a.z * b); }
	inline Dir_force operator*(float b, Dir_force a) noexcept { return Dir_force(a.x * b, a.y * b, a.z * b); }
	inline Velocity  operator*(Velocity a, float b) noexcept { return Velocity(a.x * b, a.y * b, a.z * b); }
	inline Velocity  operator*(float b, Velocity a) noexcept { return Velocity(a.x * b, a.y * b, a.z * b); }
	inline Acceleration operator*(Acceleration a, float b) noexcept
	{
		return Acceleration(a.x * b, a.y * b, a.z * b);
	}
	inline Acceleration operator*(float b, Acceleration a) noexcept
	{
		return Acceleration(a.x * b, a.y * b, a.z * b);
	}

	inline Position operator*(Distance v, glm::vec3 normal) noexcept
	{
		return Position(normal.x * v, normal.y * v, normal.z * v);
	}
	inline Position operator*(glm::vec3 normal, Distance v) noexcept
	{
		return Position(normal.x * v, normal.y * v, normal.z * v);
	}
	inline Dir_force operator*(Force v, glm::vec3 normal) noexcept
	{
		return Dir_force(normal.x * v, normal.y * v, normal.z * v);
	}
	inline Dir_force operator*(glm::vec3 normal, Force v) noexcept
	{
		return Dir_force(normal.x * v, normal.y * v, normal.z * v);
	}
	inline Velocity operator*(Speed v, glm::vec3 normal) noexcept
	{
		return Velocity(normal.x * v, normal.y * v, normal.z * v);
	}
	inline Velocity operator*(glm::vec3 normal, Speed v) noexcept
	{
		return Velocity(normal.x * v, normal.y * v, normal.z * v);
	}
	inline Acceleration operator*(Speed_per_time v, glm::vec3 normal) noexcept
	{
		return Acceleration(normal.x * v, normal.y * v, normal.z * v);
	}
	inline Acceleration operator*(glm::vec3 normal, Speed_per_time v) noexcept
	{
		return Acceleration(normal.x * v, normal.y * v, normal.z * v);
	}

	constexpr Time_squared operator*(Time a, Time b) noexcept { return Time_squared(a.value() * b.value()); }
	constexpr Speed        operator/(Distance s, Time t) noexcept { return Speed(s.value() / t.value()); }
	constexpr Distance     operator*(Speed s, Time t) noexcept { return Distance(s.value() * t.value()); }
	constexpr Speed_per_time operator/(Speed v, Time t) noexcept
	{
		return Speed_per_time(v.value() / t.value());
	}
	constexpr Speed_per_time operator/(Distance s, Time_squared t) noexcept
	{
		return Speed_per_time(s.value() / t.value());
	}
	constexpr Speed operator*(Speed_per_time a, Time t) noexcept { return Speed(a.value() * t.value()); }

	constexpr Angle operator*(Angle_velocity at, Time t) noexcept { return Angle(at.value() * t.value()); }
	constexpr Angle_velocity operator/(Angle a, Time t) noexcept
	{
		return Angle_velocity(a.value() / t.value());
	}

	constexpr Angle_acceleration operator/(Angle_velocity a, Time t) noexcept
	{
		return Angle_acceleration(a.value() / t.value());
	}
	constexpr Angle_acceleration operator/(Angle a, Time_squared t) noexcept
	{
		return Angle_acceleration(a.value() / t.value());
	}
	constexpr Angle_velocity operator*(Angle_acceleration at, Time t) noexcept
	{
		return Angle_velocity(at.value() * t.value());
	}
	constexpr Angle operator*(Angle_acceleration at, Time_squared t) noexcept
	{
		return Angle(at.value() * t.value());
	}


	inline Velocity operator*(Acceleration a, Time t) noexcept { return Velocity(a.x * t, a.y * t, a.z * t); }
	inline Velocity operator/(Position a, Time t) noexcept { return {a.x / t, a.y / t, a.z / t}; }
	inline Position operator*(Velocity v, Time t) noexcept { return Position(v.x * t, v.y * t, v.z * t); }
	inline Position operator*(Time t, Velocity v) noexcept { return Position(v.x * t, v.y * t, v.z * t); }

	constexpr Inv_mass  operator/(float a, Mass b) noexcept { return Inv_mass(a / b.value()); }
	constexpr Mass      operator/(float a, Inv_mass b) noexcept { return Mass(a / b.value()); }
	constexpr Speed     operator*(Inv_mass a, Force b) noexcept { return Speed(a.value() * b.value()); }
	constexpr Speed     operator*(Force b, Inv_mass a) noexcept { return Speed(a.value() * b.value()); }
	inline Acceleration operator*(Inv_mass a, Dir_force b) noexcept
	{
		return {a.value() * b.x.value(), a.value() * b.y.value(), a.value() * b.z.value()};
	}
	inline Acceleration operator*(Dir_force b, Inv_mass a) noexcept
	{
		return {a.value() * b.x.value(), a.value() * b.y.value(), a.value() * b.z.value()};
	}
	inline Acceleration operator/(Dir_force b, Mass a) noexcept
	{
		return {b.x.value() / a.value(), b.y.value() / a.value(), a.value() / b.z.value()};
	}

	inline Force     operator*(Speed_per_time a, Mass b) noexcept { return Force(a.value() * b.value()); }
	inline Force     operator/(Speed_per_time a, Inv_mass b) noexcept { return Force(a.value() / b.value()); }
	inline Dir_force operator/(Velocity a, Inv_mass b) noexcept
	{
		return {Force(a.x.value() / b.value()),
		        Force(a.y.value() / b.value()),
		        Force(a.z.value() / b.value())};
	}


	template <typename T, typename = std::enable_if_t<!is_value_type<T>::value>>
	inline auto remove_unit(T v) noexcept -> T
	{
		return v;
	}
	template <typename T, typename = std::enable_if_t<is_value_type<T>::value>>
	inline auto remove_unit(const Value_type<T>& v) noexcept -> decltype(v.value())
	{
		return v.value();
	}
	template <typename T>
	inline glm::vec2 remove_units(glm::tvec2<T, glm::highp> v) noexcept
	{
		return glm::vec2(remove_unit(v.x), remove_unit(v.y));
	}
	template <typename T>
	inline glm::vec3 remove_units(glm::tvec3<T, glm::highp> v) noexcept
	{
		return glm::vec3(remove_unit(v.x), remove_unit(v.y), remove_unit(v.z));
	}

	template <typename T>
	inline glm::tvec2<T, glm::highp> clamp(glm::tvec2<T, glm::highp> v,
	                                       glm::tvec2<T, glm::highp> min,
	                                       glm::tvec2<T, glm::highp> max) noexcept
	{
		return glm::tvec2<T, glm::highp>(clamp(v.x, min.x, max.x), clamp(v.y, min.y, max.y));
	}
	template <typename T>
	inline glm::tvec3<T, glm::highp> clamp(glm::tvec3<T, glm::highp> v,
	                                       glm::tvec3<T, glm::highp> min,
	                                       glm::tvec3<T, glm::highp> max) noexcept
	{
		return glm::tvec3<T, glm::highp>(
		        clamp(v.x, min.x, max.x), clamp(v.y, min.y, max.y), clamp(v.z, min.z, max.z));
	}

	template <typename T>
	inline auto rotate(glm::tvec2<T, glm::highp> v, Angle a) noexcept -> glm::tvec2<T, glm::highp>
	{
		auto r = glm::rotate(remove_units(v), a.value());
		return {T{r.x}, T{r.y}};
	}
	template <typename T>
	inline auto rotate(glm::tvec3<T, glm::highp> v, Angle a, glm::vec3 axis) noexcept
	        -> glm::tvec3<T, glm::highp>
	{
		auto r = glm::rotate(remove_units(v), a.value(), axis);
		return {T{r.x}, T{r.y}, T{r.z}};
	}

	template <typename T>
	inline constexpr bool is_zero(glm::tvec2<T, glm::highp> v) noexcept
	{
		return v.x == 0.f && v.y == 0.f;
	}
	template <typename T>
	inline constexpr bool is_zero(glm::tvec3<T, glm::highp> v) noexcept
	{
		return v.x == 0.f && v.y == 0.f && v.z == 0.f;
	}

	inline Distance distance_squared(const Position a, const Position b) noexcept
	{
		return Distance{(a.x.value() - b.x.value()) * (a.x.value() - b.x.value())
		                + (a.y.value() - b.y.value()) * (a.y.value() - b.y.value())
		                + (a.z.value() - b.z.value()) * (a.z.value() - b.z.value())};
	}

	namespace unit_literals {

		constexpr Angle operator"" _deg(long double v) { return Angle(static_cast<float>(v) * PI / 180); }
		constexpr Angle operator"" _deg(unsigned long long v)
		{
			return Angle(static_cast<float>(v) * PI / 180);
		}
		constexpr Angle operator"" _rad(long double v) { return Angle(static_cast<float>(v)); }
		constexpr Angle operator"" _rad(unsigned long long v) { return Angle(static_cast<float>(v)); }

		constexpr Distance operator"" _cm(long double v) { return Distance(static_cast<float>(v / 100)); }
		constexpr Distance operator"" _cm(unsigned long long v)
		{
			return Distance(static_cast<float>(v) / 100.f);
		}
		constexpr Distance operator"" _m(long double v) { return Distance(static_cast<float>(v)); }
		constexpr Distance operator"" _m(unsigned long long v) { return Distance(static_cast<float>(v)); }
		constexpr Distance operator"" _km(long double v) { return Distance(static_cast<float>(v * 1000)); }
		constexpr Distance operator"" _km(unsigned long long v)
		{
			return Distance(static_cast<float>(v * 1000));
		}

		constexpr Time operator"" _ms(long double v) { return Time(static_cast<float>(v / 1000)); }
		constexpr Time operator"" _ms(unsigned long long v) { return Time(static_cast<float>(v) / 1000.f); }
		constexpr Time operator"" _s(long double v) { return Time(static_cast<float>(v)); }
		constexpr Time operator"" _s(unsigned long long v) { return Time(static_cast<float>(v)); }
		constexpr Time operator"" _min(long double v) { return Time(static_cast<float>(v * 60)); }
		constexpr Time operator"" _min(unsigned long long v) { return Time(static_cast<float>(v * 60)); }
		constexpr Time operator"" _h(long double v) { return Time(static_cast<float>(v * 60 * 60)); }
		constexpr Time operator"" _h(unsigned long long v) { return Time(static_cast<float>(v * 60 * 60)); }

		constexpr Mass operator"" _g(long double v) { return Mass(static_cast<float>(v)); }
		constexpr Mass operator"" _g(unsigned long long v) { return Mass(static_cast<float>(v)); }
		constexpr Mass operator"" _kg(long double v) { return Mass(static_cast<float>(v * 1000)); }
		constexpr Mass operator"" _kg(unsigned long long v) { return Mass(static_cast<float>(v * 1000)); }

		constexpr Force operator"" _n(long double v) { return Force(static_cast<float>(v)); }
		constexpr Force operator"" _n(unsigned long long v) { return Force(static_cast<float>(v)); }
		constexpr Force operator"" _kn(long double v) { return Force(static_cast<float>(v * 1000)); }
		constexpr Force operator"" _kn(unsigned long long v) { return Force(static_cast<float>(v * 1000)); }

		constexpr Time         second   = 1_s;
		constexpr Time_squared second_2 = 1_s * 1_s;
		constexpr Time         minute   = 1_min;
		constexpr Time_squared minute_2 = 1_min * 1_min;
		constexpr Time         hour     = 1_h;
		constexpr Time_squared hour_2   = 1_h * 1_h;
	} // namespace unit_literals

	inline Angle normalize_to_half_rot(Angle a) noexcept
	{
		using namespace unit_literals;
		while(a <= -180_deg)
			a += 360_deg;
		while(a > 180_deg)
			a -= 360_deg;
		return a;
	}
} // namespace mirrage::util
