/** Type-safe range-constraining wrappers around fundamental types ***********
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/log.hpp>

#include <cstdint>
#include <iosfwd>
#include <limits>

namespace mirrage::util {

	/**
	 * Types:
	 *   - Int<Min, Max>
	 *        automatically calculates appropriate type and generates range-checks
	 *
	 *   - Real<MinN=0, MinD=1, MaxN=1, MaxD=1, Type=float>
	 *        float/double (=Type) with auto generated range-checks; MinN/MinD <= x <= MaxN/MaxD
	 *   - Flag
	 *        safe boolean wrapper
	 *
	 * Example:
	 *   auto health = Int<0, 100>(100);
	 *   health /= 2;  // ok
	 *   health *= -1; // underflow error
	 *   std::cout << "Health=" << health << " is between " << health::min << " and " << health::max;
	 *
	 *   auto scale = Real<1,100, 10>(1);
	 *   scale /=  10; // ok
	 *   scale *= 100; // ok
	 *   scale +=   1; // overflow error
	 *   std::cout << "Scale=" << scale << " is between " << scale::min << " and " << scale::max;
	 *
	 *   auto fullscreen = Flag(true);
	 *   fullscreen.flip();                    // ok, fullscreen is false now
	 *   fullscreen = true;                    // ok
	 *   fullscreen++;                         // error
	 *   int x = fullscreen;                   // error
	 *   int x = static_cast<int>(fullscreen); // ok, x=1
	 */

	namespace detail {
		template <std::uint64_t Value, bool SignRequired>
		constexpr auto compatible_type()
		{
			if constexpr(Value <= std::numeric_limits<std::int8_t>::max()) {
				return std::int8_t(0);
			} else if constexpr(Value <= std::numeric_limits<std::int16_t>::max()) {
				return std::int16_t(0);
			} else if constexpr(Value <= std::numeric_limits<std::int32_t>::max()) {
				return std::int32_t(0);
			} else if constexpr(Value <= std::numeric_limits<std::int64_t>::max()) {
				return std::int64_t(0);
			} else if(!SignRequired) {
				static_assert(!SignRequired, "No integer type supporting more than 64 Bits");
				return std::uint64_t(0);
			}
		}

		template <std::uint64_t Value, bool SignRequired>
		using compatible_type_t = decltype(compatible_type<Value, SignRequired>());

		template <typename T>
		struct Out_wrapper {
			T value;
		};

		template <typename T>
		std::ostream& operator<<(std::ostream&, Out_wrapper<T>);

		extern template std::ostream& operator<<(std::ostream&, Out_wrapper<std::int64_t>);
		extern template std::ostream& operator<<(std::ostream&, Out_wrapper<std::uint64_t>);
		extern template std::ostream& operator<<(std::ostream&, Out_wrapper<float>);
		extern template std::ostream& operator<<(std::ostream&, Out_wrapper<double>);

	} // namespace detail


	template <std::int64_t Min, std::uint64_t Max>
	class Int {
	  public:
		using integer_type = detail::compatible_type_t<Max, (Min < 0)>;

		static constexpr auto is_signed = std::numeric_limits<integer_type>::is_signed;
		static constexpr auto min       = Min;
		static constexpr auto max       = Max;


		constexpr Int(integer_type value) : _value(value)
		{
#ifndef NDEBUG
			if(_value < Min)
				MIRRAGE_FAIL("Integer underflow assigning " << _value << " below " << Min);

			if(_value > Max)
				MIRRAGE_FAIL("Integer underflow assigning " << _value << " exceeds " << Max);
#endif
		}
		constexpr Int(const Int& rhs) = default;

		template <std::int64_t OtherMin, std::uint64_t OtherMax>
		constexpr Int(Int<OtherMin, OtherMax> value) : _value(static_cast<integer_type>(value._value))
		{
#ifndef NDEBUG
			if constexpr(OtherMin < Min)
				if(_value < Min)
					MIRRAGE_FAIL("Integer underflow assigning " << _value << " below " << Min);

			if constexpr(OtherMax > Max)
				if(_value > Max)
					MIRRAGE_FAIL("Integer underflow assigning " << _value << " exceeds " << Max);
#endif
		}


		constexpr Int& operator=(integer_type value)
		{
			_value = value;
#ifndef NDEBUG
			if(_value < Min)
				MIRRAGE_FAIL("Integer underflow assigning " << _value << " below " << Min);

			if(_value > Max)
				MIRRAGE_FAIL("Integer underflow assigning " << _value << " exceeds " << Max);
#endif
			return *this;
		}
		constexpr Int& operator=(const Int& rhs) = default;

		template <std::int64_t OtherMin, std::uint64_t OtherMax>
		constexpr Int& operator=(Int<OtherMin, OtherMax> value)
		{
			_value = static_cast<integer_type>(value._value);
#ifndef NDEBUG
			if constexpr(OtherMin < Min)
				if(_value < Min)
					MIRRAGE_FAIL("Integer underflow assigning " << _value << " below " << Min);

			if constexpr(OtherMax > Max)
				if(_value > Max)
					MIRRAGE_FAIL("Integer underflow assigning " << _value << " exceeds " << Max);
#endif
			return *this;
		}

		constexpr operator integer_type() const noexcept { return _value; }

		constexpr Int operator-() const noexcept
		{
			static_assert(is_signed, "Can't call unary minus on unsigned type!");
			static_assert(Min >= 0, "Can't call unary minus on type with Min>=0");
			return -_value;
		}

		constexpr Int operator++() noexcept
		{
#ifndef NDEBUG
			if(_value >= Max)
				MIRRAGE_FAIL("Integer overflow incrementing " << _value << " exceeds " << Max);
#endif
			++_value;
			return *this;
		}
		constexpr Int operator++(int) noexcept
		{
			auto tmp = *this;
			++(*this);
			return tmp;
		}

		constexpr Int operator--() noexcept
		{
#ifndef NDEBUG
			if(_value <= Min)
				MIRRAGE_FAIL("Integer underflow decrementing " << _value << " below " << Min);
#endif
			--_value;
			return *this;
		}
		constexpr Int operator--(int) noexcept
		{
			auto tmp = *this;
			--(*this);
			return tmp;
		}


		friend std::ostream& operator<<(std::ostream& os, const Int& v)
		{
			if constexpr(is_signed)
				return os << detail::Out_wrapper<std::int64_t>(static_cast<std::int64_t>(v._value));
			else
				return os << detail::Out_wrapper<std::uint64_t>(static_cast<std::uint64_t>(v._value));
		}

	  private:
		integer_type _value;
	};

	template <std::int64_t MinLhs, std::uint64_t MaxLhs, std::int64_t MinRhs, std::uint64_t MaxRhs>
	auto operator+(Int<MinLhs, MaxLhs> lhs, Int<MinRhs, MaxRhs> rhs)
	{
		constexpr auto min = MinLhs + MinRhs;
		constexpr auto max = MaxLhs + MaxRhs;
		using T            = detail::compatible_type_t<max, (min < 0 || lhs.is_signed || rhs.is_signed)>;

		return Int<min, max>(static_cast<T>(lhs._value) + static_cast<T>(rhs._value));
	}
	// TODO: integer operators if/as required


	template <std::int64_t MinN = 0,
	          std::int64_t MinD = 1,
	          std::int64_t MaxN = 1,
	          std::int64_t MaxD = 1,
	          typename Type     = float>
	class Real {
		static_assert(MinD > 0, "Min denominator has to be >=1");
		static_assert(MaxD > 0, "Max denominator has to be >=1");

	  public:
		using real_type = Type;

		static constexpr auto min = static_cast<Type>(MinN) / static_cast<Type>(MinD);
		static constexpr auto max = static_cast<Type>(MaxN) / static_cast<Type>(MaxD);

		constexpr explicit Real(real_type value) : _value(value)
		{
#ifndef NDEBUG
			if(_value < min)
				MIRRAGE_FAIL("Integer underflow assigning " << _value << " below " << min);

			if(_value > max)
				MIRRAGE_FAIL("Integer underflow assigning " << _value << " exceeds " << max);
#endif
		}
		constexpr Real(const Real& rhs) = default;

		template <std::int64_t OtherMinN,
		          std::int64_t OtherMinD,
		          std::int64_t OtherMaxN,
		          std::int64_t OtherMaxD,
		          typename OtherType>
		constexpr Real(Real<OtherMinN, OtherMinD, OtherMaxN, OtherMaxD, OtherType> value)
		  : _value(static_cast<real_type>(value._value))
		{
#ifndef NDEBUG
			if constexpr(decltype(value)::min < min)
				if(_value < min)
					MIRRAGE_FAIL("Integer underflow assigning " << _value << " below " << min);

			if constexpr(decltype(value)::max > max)
				if(_value > max)
					MIRRAGE_FAIL("Integer underflow assigning " << _value << " exceeds " << max);
#endif
		}

		constexpr Real& operator=(real_type value)
		{
			_value = value;
#ifndef NDEBUG
			if(_value < min)
				MIRRAGE_FAIL("Integer underflow assigning " << _value << " below " << min);

			if(_value > max)
				MIRRAGE_FAIL("Integer underflow assigning " << _value << " exceeds " << max);
#endif
			return *this;
		}
		constexpr Real& operator=(const Real& rhs) = default;

		template <std::int64_t OtherMinN,
		          std::int64_t OtherMinD,
		          std::int64_t OtherMaxN,
		          std::int64_t OtherMaxD,
		          typename OtherType>
		constexpr Real& operator=(Real<OtherMinN, OtherMinD, OtherMaxN, OtherMaxD, OtherType> value)
		{
			_value = static_cast<real_type>(value._value);
#ifndef NDEBUG
			if constexpr(decltype(value)::min < min)
				if(_value < min)
					MIRRAGE_FAIL("Integer underflow assigning " << _value << " below " << min);

			if constexpr(decltype(value)::max > max)
				if(_value > max)
					MIRRAGE_FAIL("Integer underflow assigning " << _value << " exceeds " << max);
#endif
		}

		constexpr operator real_type() const noexcept { return _value; }

		constexpr Real operator-() const noexcept
		{
			static_assert(min >= 0, "Can't call unary minus on type with Min>=0");
			return -_value;
		}


		friend std::ostream& operator<<(std::ostream& os, const Real& v)
		{
			return os << detail::Out_wrapper<Type>(v._value);
		}

	  private:
		Type _value;
	};

	// TODO: real operators if/as required


	class Flag {
	  public:
		constexpr Flag(bool v) : _value(v) {}

		constexpr explicit operator bool() const noexcept { return _value; }

		constexpr auto flip() noexcept
		{
			_value = !_value;
			return *this;
		}
		constexpr auto operator=(bool v) noexcept
		{
			_value = v;
			return *this;
		}

	  private:
		bool _value;
	};

	extern std::ostream& operator<<(std::ostream&, const Flag&);

} // namespace mirrage::util
