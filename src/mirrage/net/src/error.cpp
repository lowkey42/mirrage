#include <mirrage/net/error.hpp>

#include <mirrage/error.hpp>

#include <mirrage/utils/log.hpp>


namespace mirrage::net {

	namespace {
		class Error_type_category final : public std::error_category {
		  public:
			const char* name() const noexcept override { return "mirrage-network-error"; }

			std::string message(int e) const override {
				switch(static_cast<Net_error>(e)) {
					case Net_error::unknown_host:
						return "Tried to connect to hostname that couldn't be resolved.";
					case Net_error::unknown_channel:
						return "The requested channel hasn't been declared.";
					case Net_error::connection_error: return "Couldn't connect to a server.";
					case Net_error::not_connected:
						return "Tried to send a packet without being connected.";
					case Net_error::unspecified_network_error:
						return "ENet failed and didn't tell us why!";
				}

				MIRRAGE_WARN("Unexpected error_source: " << e);
				return "[unexpected " + std::to_string(e) + "]";
			}

			std::error_condition default_error_condition(int e) const noexcept override {
				switch(static_cast<Net_error>(e)) {
					case Net_error::unknown_host: return Error_type::network_invalid_host;
					case Net_error::unknown_channel: return Error_type::network_usage_error;
					case Net_error::connection_error: return Error_type::network_invalid_host;
					case Net_error::not_connected: return Error_type::network_usage_error;
					case Net_error::unspecified_network_error:
						return Error_type::network_unkown_error;
				}

				MIRRAGE_FAIL("Unexpected Net_error: " << e);
			}
		};
		const Error_type_category error_cat{};
	} // namespace

	std::error_code make_error_code(Net_error e) { return {static_cast<int>(e), error_cat}; }

} // namespace mirrage::net
