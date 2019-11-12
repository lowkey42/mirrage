/** helper functions for c++ random API **************************************
 *                                                                           *
 * Copyright (c) 2019 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <random>

namespace mirrage::util {

	using default_rand = std::mt19937_64;

	struct random_seed_seq {
		using result_type = std::random_device::result_type;

		template <typename It>
		void generate(It begin, It end)
		{
			for(; begin != end; ++begin) {
				*begin = device();
			}
		}

		static auto& get_instance()
		{
			static thread_local random_seed_seq result;
			return result;
		}

	  private:
		std::random_device device;
	};

	inline auto construct_random_engine() { return default_rand(random_seed_seq::get_instance()); }

} // namespace mirrage::util
