#include <mirrage/error.hpp>

#include <mirrage/utils/log.hpp>

#include <string>


namespace mirrage {

	namespace {
		class Error_type_category final : public std::error_category {
		  public:
			const char* name() const noexcept override { return "mirrage-error"; }

			std::string message(int e) const override
			{
				switch(static_cast<Error_type>(e)) {
					case Error_type::asset_not_found: return "Couldn't load a required asset";
					case Error_type::asset_io_error: return "Couldn't access filesystem.";
					case Error_type::asset_usage_error:
						return "Asset manager or I/O-system was used incorrectly.";
					case Error_type::network_invalid_host:
						return "Tried to connect to an unknown or unreachable host";
					case Error_type::network_usage_error: return "Programming bug when using network API";
					case Error_type::network_unkown_error: return "Unknown error in network Code";
				}

				LOG(plog::warning) << "Unexpected Error_type: " << e;
				return "[unexpected Error_type " + std::to_string(e) + "]";
			}
		};
		const Error_type_category error_type_cat{};


		class Error_source_category final : public std::error_category {
		  public:
			const char* name() const noexcept override { return "mirrage-error-source"; }

			std::string message(int e) const override
			{
				switch(static_cast<Error_source>(e)) {
					case Error_source::user: return "Error caused by user input";
					case Error_source::hardware: return "Error caused by system/hardware";
					case Error_source::bug: return "Error caused by bug";
					case Error_source::unknown:
						return "Error that can't be pinned down to a single possible source";
				}

				LOG(plog::warning) << "Unexpected error_source: " << e;
				return "[unexpected error_source " + std::to_string(e) + "]";
			}

			bool equivalent(const std::error_code& code, int condition) const noexcept override
			{
				const auto is_user     = code == Error_type::network_invalid_host;
				const auto is_hardware = code == Error_type::asset_io_error;
				const auto is_bug      = code == Error_type::network_usage_error
				                    || code == Error_type::asset_not_found
				                    || code == Error_type::asset_usage_error;

				switch(static_cast<Error_source>(condition)) {
					case Error_source::user: return is_user;
					case Error_source::hardware: return is_hardware;
					case Error_source::bug: return is_bug;

					case Error_source::unknown: return !(is_user || is_hardware || is_bug);
				}

				return false;
			}
		};
		const Error_type_category error_source_cat{};
	} // namespace

	std::error_condition make_error_condition(Error_type e) { return {static_cast<int>(e), error_type_cat}; }
	std::error_condition make_error_condition(Error_source e)
	{
		return {static_cast<int>(e), error_source_cat};
	}

} // namespace mirrage
