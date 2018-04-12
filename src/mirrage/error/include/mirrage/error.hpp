/** global error conditions supported by subsystems **************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <system_error>


namespace mirrage {

	enum class Error_type {
		asset_not_found = 1,
		asset_io_error,
		asset_usage_error,
		network_invalid_host,
		network_usage_error,
		network_unkown_error
		// ...
	};

	// specifies the likeliest source of an error
	enum class Error_source {
		user = 1, // they screwed up
		hardware, // their stuff screwed up
		bug,      // we screwed up
		unknown   // everyone screwed up
	};

	extern std::error_condition make_error_condition(Error_type e);
	extern std::error_condition make_error_condition(Error_source e);

} // namespace mirrage

namespace std {
	template <>
	struct is_error_condition_enum<mirrage::Error_type> : true_type {};

	template <>
	struct is_error_condition_enum<mirrage::Error_source> : true_type {};
} // namespace std
