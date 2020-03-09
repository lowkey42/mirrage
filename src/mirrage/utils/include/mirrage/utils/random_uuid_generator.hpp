/*
 * Copyright 2020 by Stefan Bodenschatz
 */
#ifndef MIRRAGE_UTIL_RANDOM_UUID_GENERATOR_INCLUDED
#define MIRRAGE_UTIL_RANDOM_UUID_GENERATOR_INCLUDED

#include <mirrage/utils/uuid.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdint>
#include <functional>
#include <mutex>
#include <random>
#include <type_traits>

namespace mirrage::util {

	class random_uuid_generator {
		std::mt19937_64 rng;

		template <typename RNG>
		static RNG seed_rng()
		{
			static std::mutex rd_mutex;
			// A shared random_device is used to ensure intra-node entropy even for a deterministic random_device:
			static std::random_device rd;
			constexpr auto            seed_element_bytes = 32 / CHAR_BIT;
			constexpr auto            seed_size =
			        RNG::state_size * sizeof(typename RNG::result_type) / seed_element_bytes;
			std::array<std::uint_least32_t, seed_size> seed_data;
			// Include a time-based seed for cases where the standard library implementation uses a deterministic
			// random_device (e.g. due to lack of hardware / OS support):
			auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(
			        std::chrono::high_resolution_clock::now().time_since_epoch());
			auto time_seed = static_cast<std::make_unsigned_t<decltype(time)::rep>>(time.count());
			static_assert(seed_size > sizeof(time_seed));
			seed_data[1] = time_seed & ((1ull << 32) - 1);
			time_seed >>= 32;
			seed_data[0] = time_seed & ((1ull << 32) - 1);
			{
				std::lock_guard lock(rd_mutex);
				// Fill the rest of the seed with data from random_device:
				std::generate(seed_data.begin() + 2, seed_data.end(), std::ref(rd));
			}
			std::seed_seq seq(seed_data.begin(), seed_data.end());
			// Seed the RNG with the generated data and return it:
			return RNG(seq);
		}

		auto random_fill_octets()
		{
			decltype(id_type::octets) octets;
			for(std::size_t out_octet = 0; out_octet < octets.size();) {
				std::uint64_t rand = rng();
				for(std::size_t in_octet = 0;
				    in_octet < decltype(rng)::word_size / 8 && out_octet < octets.size();
				    ++in_octet, ++out_octet) {
					auto rand_comp    = rand & (static_cast<std::uint64_t>(0xFF) << (in_octet * 8));
					octets[out_octet] = static_cast<std::uint8_t>(rand_comp >> (in_octet * 8));
				}
			}
			return octets;
		}

	  public:
		using id_type = uuid;
		random_uuid_generator() : rng(seed_rng<decltype(rng)>()) {}
		id_type generate_id()
		{
			id_type id{random_fill_octets()};
			id.variant(0b10);
			id.version(4);
			return id;
		}
	};

} // namespace mirrage::util

#endif // MIRRAGE_UTIL_RANDOM_UUID_GENERATOR_INCLUDED
