/** encodes strings as unique integers ***************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <iosfwd>
#include <stdexcept>
#include <string>

namespace mirrage::util {

	namespace detail {
		constexpr char str_id_step = 28;

		constexpr std::size_t str_id_max_length = 13;

		constexpr auto str_id_hash(const char* str) {
			using namespace std::string_literals;

			auto id = uint64_t(0);

			for(std::size_t i = 0; str[i] != 0; ++i) {
				if(str[i] == '_')
					id = (id * str_id_step) + 1;
				else if(str[i] >= 'a' && str[i] <= 'z')
					id = (id * str_id_step) + static_cast<uint64_t>(str[i] - 'a' + 2);
				else
					throw std::invalid_argument("Unexpected character '"s + str[i]
					                            + "' in string: " + str);

				if(i >= str_id_max_length) {
					throw std::invalid_argument("String is too long: "s + str);
				}
			}

			return id;
		}
	} // namespace detail

	class Str_id {
	  public:
		using int_type = uint64_t;

		explicit Str_id(const std::string& str) : Str_id(str.c_str()) {}
		explicit constexpr Str_id(const char* str = "") : _id(detail::str_id_hash(str)) {}
		explicit constexpr Str_id(int_type id) : _id(id) {}

		auto str() const -> std::string {
			std::string r;

			auto id = _id;
			while(id) {
				auto nid = id / detail::str_id_step;
				auto c   = id - (nid * detail::str_id_step);
				id       = nid;

				if(c == 1) {
					r += '_';
				} else {
					r += static_cast<char>(c + 'a' - 2);
				}
			}

			std::reverse(r.begin(), r.end());
			return r;
		}

		constexpr bool operator==(const Str_id& rhs) const noexcept { return _id == rhs._id; }
		constexpr bool operator!=(const Str_id& rhs) const noexcept { return _id != rhs._id; }
		constexpr bool operator<(const Str_id& rhs) const noexcept { return _id < rhs._id; }
		constexpr      operator int_type() const noexcept { return _id; }

	  private:
		int_type _id;
	};

	inline std::ostream& operator<<(std::ostream& s, const Str_id& id) {
		s << id.str();
		return s;
	}

#ifdef sf2_structDef
	inline void load(sf2::JsonDeserializer& s, Str_id& v) {
		std::string str;
		s.read_value(str);
		v = Str_id(str);
	}

	inline void save(sf2::JsonSerializer& s, const Str_id& v) { s.write_value(v.str()); }
#endif
} // namespace mirrage::util

inline constexpr mirrage::util::Str_id operator"" _strid(const char* str, std::size_t) {
	return mirrage::util::Str_id(str);
}

namespace std {
	template <>
	struct hash<mirrage::util::Str_id> {
		constexpr size_t operator()(mirrage::util::Str_id id) const noexcept { return id; }
	};
} // namespace std
