#pragma once

#include <system_error>


namespace mirrage::net {

	enum class Net_error {
		unknown_host = 1,
		unknown_channel,
		connection_error,
		not_connected,
		unspecified_network_error
	};

	extern std::error_code make_error_code(Net_error e);

} // namespace mirrage::net

namespace std {
	template <>
	struct is_error_code_enum<mirrage::net::Net_error> : true_type {
	};
} // namespace std
