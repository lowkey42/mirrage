/** portable random api (based on clangs libcpp) *****************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <ctime>
#include <random>

namespace mirrage::util {

	namespace details {

		template <unsigned long long xp, size_t rp>
		struct log2_imp {
			static const size_t value =
			        xp & ((unsigned long long) (1) << rp) ? rp : log2_imp<xp, rp - 1>::value;
		};

		template <unsigned long long xp>
		struct log2_imp<xp, 0> {
			static const size_t value = 0;
		};

		template <size_t rp>
		struct log2_imp<0, rp> {
			static const size_t value = rp + 1;
		};

		template <class U, U xp>
		struct log2 {
			static const size_t value = log2_imp<xp, sizeof(U) * __CHAR_BIT__ - 1>::value;
		};

		template <class T, size_t bits, class Generator>
		auto generate_canonical(Generator& gen) -> T {
			constexpr auto dt    = std::numeric_limits<T>::digits;
			constexpr auto b     = dt < bits ? dt : bits;
			constexpr auto log_r = log2<uint64_t, Generator::max() - Generator::min() + uint64_t(1)>::value;
			constexpr auto k     = b / log_r + (b % log_r != 0) + (b == 0);
			constexpr auto rp    = Generator::max() - Generator::min() + T(1);
			auto           base  = rp;
			auto           sp    = gen() - Generator::min();
			for(size_t i = 1; i < k; ++i, base *= rp)
				sp += (gen() - Generator::min()) * base;
			return static_cast<T>(sp / base);
		}
	} // namespace details

	using random_generator = std::mt19937_64;

	inline auto create_random_generator() -> random_generator {
		//static std::random_device rd;
		return random_generator(std::time(0));
	}

	template <class T, class Generator>
	auto random_real(Generator& gen, T min, T max) -> T {
		return (max - min) * details::generate_canonical<T, std::numeric_limits<T>::digits>(gen) + min;
	}

	template <class T, class Generator>
	auto random_int(Generator& gen, T min, T max) -> T {
		auto v = (max - min) * details::generate_canonical<float, std::numeric_limits<T>::digits>(gen) + min;

		return v;
	}

	/*bernoulli_distribution*/
	template <class Generator>
	auto random_bool(Generator& gen, float prop) {
		auto v = random_real(gen, 0.f, 1.f);
		return v < prop;
	}
} // namespace mirrage::util
