/*
 * Copyright 2020 by Stefan Bodenschatz
 */
#ifndef MIRRAGE_UTIL_UUID_INCLUDED
#define MIRRAGE_UTIL_UUID_INCLUDED

#include <array>
#include <cstdint>
#include <iostream>

namespace mirrage::util {

	struct uuid {
		std::array<std::uint8_t, 16> octets;

		unsigned int version() const { return (octets[6] & (0b1111 << 4)) >> 4; }
		void         version(unsigned int v) { octets[6] = ((v & 0b1111) << 4) | (octets[6] & 0b1111); }

		unsigned int variant() const
		{
			auto var_bits = (octets[8] & (0b111u << 5)) >> 5;
			switch(var_bits) {
				case 0b100: [[fallthrough]];
				case 0b101: return 0b10;
				case 0b110: return 0b110;
				case 0b111: return 0b111;
				default: return 0;
			}
		}

		void variant(unsigned int v)
		{
			switch(v) {
				case 0: octets[8] &= 0b0'111'1111; break;
				case 0b10: octets[8] = (octets[8] & 0b00'11'1111) | 0b10'00'0000; break;
				default: octets[8] = (octets[8] & 0b000'1'1111) | ((v & 0b111) << 5); break;
			}
		}

		bool operator==(const uuid& rhs) const { return octets == rhs.octets; }
		bool operator!=(const uuid& rhs) const { return octets != rhs.octets; }
		bool operator<(const uuid& rhs) const { return octets < rhs.octets; }
		bool operator<=(const uuid& rhs) const { return octets <= rhs.octets; }
		bool operator>(const uuid& rhs) const { return octets > rhs.octets; }
		bool operator>=(const uuid& rhs) const { return octets >= rhs.octets; }

		friend std::ostream& operator<<(std::ostream& ostr, const uuid& g)
		{
			const auto& o = g.octets;
			return ostr << hex_group<4>{{o[0], o[1], o[2], o[3]}} << "-" << hex_group<2>{{o[4], o[5]}} << "-"
			            << hex_group<2>{{o[6], o[7]}} << "-" << hex_group<2>{{o[8], o[9]}} << "-"
			            << hex_group<6>{{o[10], o[11], o[12], o[13], o[14], o[15]}};
		}

	  private:
		template <std::size_t N>
		struct hex_group {
			std::array<std::uint8_t, N> octets;
			friend std::ostream&        operator<<(std::ostream& ostr, const hex_group& h)
			{
				static const char* digit_format = "0123456789ABCDEF";
				for(auto o : h.octets) {
					ostr << digit_format[o >> 4] << digit_format[o & 0b1111];
				}
				return ostr;
			}
		};
	};

} // namespace mirrage::util

namespace std {

	template <>
	struct hash<mirrage::util::uuid> {
		std::size_t operator()(const mirrage::util::uuid& g) const noexcept
		{
			std::size_t h = 0;
			for(std::size_t i = 0; i < sizeof(std::size_t); ++i) {
				h <<= 8; // move up one octet
				for(std::size_t j = 0; j < g.octets.size() / sizeof(std::size_t); ++j) {
					h ^= g.octets[i + j * sizeof(std::size_t)];
				}
			}
			return h;
		}
	};

} // namespace std

#endif // MIRRAGE_UTIL_UUID_INCLUDED
