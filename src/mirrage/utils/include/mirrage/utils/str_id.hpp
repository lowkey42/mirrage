/** encodes strings as unique integers ***************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <iosfwd>
#include <stdexcept>
#include <string>

namespace mirrage::util {

	namespace detail {
		constexpr char str_id_step = 38;

		constexpr std::size_t str_id_max_length = 12;

		constexpr auto str_id_hash(const char* str, bool invalid_empty = false)
		{
			using namespace std::string_literals;

			auto id = std::uint64_t(0);

			for(std::size_t i = 0; str[i] != 0; ++i) {
				if(str[i] == '_')
					id = (id * str_id_step) + 1;
				else if(str[i] >= '0' && str[i] <= '9')
					id = (id * str_id_step) + static_cast<std::uint64_t>(str[i] - '0' + 2);
				else if(str[i] >= 'a' && str[i] <= 'z')
					id = (id * str_id_step) + static_cast<std::uint64_t>(str[i] - 'a' + 2 + 10);
				else {
					if(invalid_empty)
						return std::uint64_t(0);
					else
						throw std::invalid_argument("Unexpected character '"s + str[i]
						                            + "' in string: " + str);
				}

				if(i >= str_id_max_length) {
					if(invalid_empty)
						return std::uint64_t(0);
					else
						throw std::invalid_argument("String is too long: "s + str);
				}
			}

			return id;
		}

		template <std::uint64_t N>
		constexpr auto gen_pow_38_table()
		{
			auto arr = std::array<std::uint64_t, N>();
			arr[0]   = 1;

			for(auto i = std::uint64_t(1); i < N; i++)
				arr[i] = arr[i - 1] * detail::str_id_step;

			return arr;
		}

		constexpr auto pow_38_table = gen_pow_38_table<str_id_max_length>();

	} // namespace detail

	class Str_id {
	  public:
		using int_type = uint64_t;

		explicit Str_id(const std::string& str, bool invalid_empty = false)
		  : Str_id(str.c_str(), invalid_empty)
		{
		}
		explicit constexpr Str_id(const char* str = "", bool invalid_empty = false)
		  : _id(detail::str_id_hash(str, invalid_empty))
		{
		}
		explicit constexpr Str_id(int_type id) : _id(id) {}

		auto str() const -> std::string
		{
			std::string r;

			auto id = _id;
			while(id) {
				auto nid = id / detail::str_id_step;
				auto c   = id - (nid * detail::str_id_step);
				id       = nid;

				if(c == 1) {
					r += '_';
				} else if(c < 12) {
					r += static_cast<char>('0' + (c - 2));
				} else {
					r += static_cast<char>('a' + (c - 2 - 10));
				}
			}

			std::reverse(r.begin(), r.end());
			return r;
		}

		constexpr bool operator==(const Str_id& rhs) const noexcept { return _id == rhs._id; }
		constexpr bool operator!=(const Str_id& rhs) const noexcept { return _id != rhs._id; }
		constexpr bool operator<(const Str_id& rhs) const noexcept { return _id < rhs._id; }
		constexpr      operator int_type() const noexcept { return _id; }

		constexpr auto length() const -> std::uint64_t
		{
			auto r = std::uint64_t(0);
			r += (detail::pow_38_table[8] <= _id) ? 8 : 0;
			r += (detail::pow_38_table[r + 4] <= _id) ? 4 : 0;
			r += (detail::pow_38_table[r + 2] <= _id) ? 2 : 0;
			r += (detail::pow_38_table[r + 1] <= _id) ? 1 : 0;
			return 1 + r;
		}

		friend constexpr auto operator+(const Str_id& lhs, const Str_id& rhs)
		{
			auto lhs_len = lhs.length();
			auto rhs_len = rhs.length();
			if(lhs_len + rhs_len > detail::str_id_max_length)
				throw std::invalid_argument("String is too long: " + lhs.str() + rhs.str() + "; len="
				                            + std::to_string(lhs_len) + "+" + std::to_string(rhs_len));

			return Str_id(lhs._id * detail::pow_38_table[rhs_len] + rhs._id);
		}

	  private:
		int_type _id;
	};

	static_assert(Str_id("dq_") + Str_id("default") == Str_id("dq_default"), "Str_id concat test failed");

	inline std::ostream& operator<<(std::ostream& s, const Str_id& id)
	{
		s << id.str();
		return s;
	}

#ifdef sf2_structDef
	inline void load(sf2::JsonDeserializer& s, Str_id& v)
	{
		std::string str;
		s.read_value(str);
		v = Str_id(str);
	}

	inline void save(sf2::JsonSerializer& s, const Str_id& v) { s.write_value(v.str()); }
#endif
} // namespace mirrage::util

inline constexpr mirrage::util::Str_id operator"" _strid(const char* str, std::size_t)
{
	return mirrage::util::Str_id(str);
}

namespace std {
	template <>
	struct hash<mirrage::util::Str_id> {
		constexpr size_t operator()(mirrage::util::Str_id id) const noexcept { return id; }
	};
} // namespace std
