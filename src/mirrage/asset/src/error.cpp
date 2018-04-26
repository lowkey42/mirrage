#include <mirrage/asset/error.hpp>

#include <mirrage/error.hpp>
#include <mirrage/utils/log.hpp>

#include <physfs.h>


namespace mirrage::asset {

	namespace {
		class Error_type_category final : public std::error_category {
		  public:
			const char* name() const noexcept override { return "mirrage-asset-error"; }

			std::string message(int e) const override
			{
				switch(static_cast<Asset_error>(e)) {
					case Asset_error::unspecified_error:
					case Asset_error::out_of_memory:
					case Asset_error::not_initialized:
					case Asset_error::already_initialized:
					case Asset_error::no_prog_name:
					case Asset_error::unsupported:
					case Asset_error::out_of_bound:
					case Asset_error::files_still_open:
					case Asset_error::invalid_argument:
					case Asset_error::not_mounted:
					case Asset_error::not_found:
					case Asset_error::symlink_forbidden:
					case Asset_error::no_write_dir:
					case Asset_error::open_for_reading:
					case Asset_error::open_for_writing:
					case Asset_error::not_a_file:
					case Asset_error::read_only:
					case Asset_error::corrupt:
					case Asset_error::symlink_loop:
					case Asset_error::io_error:
					case Asset_error::permission_denied:
					case Asset_error::no_space_left:
					case Asset_error::bad_filename:
					case Asset_error::busy:
					case Asset_error::dir_not_empty:
					case Asset_error::os_error:
					case Asset_error::duplicate:
					case Asset_error::bad_password:
					case Asset_error::app_callback:
						return PHYSFS_getErrorByCode(static_cast<PHYSFS_ErrorCode>(e));

					case Asset_error::resolve_failed: return "An AID couldn't be resolved to a path.";
					case Asset_error::loading_failed:
						return "Couldn't create an asset instanc from the istream.";
					case Asset_error::stateful_loader_not_initialized:
						return "The stateful loader for the asset type has not been created!";
				}

				LOG(plog::warning) << "Unexpected error_source: " << e;
				return "[unexpected " + std::to_string(e) + "]";
			}

			std::error_condition default_error_condition(int e) const noexcept override
			{
				switch(static_cast<Asset_error>(e)) {
					case Asset_error::unspecified_error:
					case Asset_error::out_of_memory:
					case Asset_error::unsupported:
					case Asset_error::files_still_open:
					case Asset_error::invalid_argument:
					case Asset_error::not_found:
					case Asset_error::symlink_forbidden:
					case Asset_error::not_a_file:
					case Asset_error::read_only:
					case Asset_error::corrupt:
					case Asset_error::symlink_loop:
					case Asset_error::io_error:
					case Asset_error::permission_denied:
					case Asset_error::no_space_left:
					case Asset_error::bad_filename:
					case Asset_error::busy:
					case Asset_error::os_error:
					case Asset_error::duplicate:
					case Asset_error::bad_password:
					case Asset_error::app_callback: return Error_type::asset_io_error;

					case Asset_error::no_prog_name:
					case Asset_error::not_initialized:
					case Asset_error::already_initialized:
					case Asset_error::out_of_bound:
					case Asset_error::no_write_dir:
					case Asset_error::dir_not_empty:
					case Asset_error::not_mounted:
					case Asset_error::open_for_reading:
					case Asset_error::open_for_writing:
					case Asset_error::stateful_loader_not_initialized: return Error_type::asset_usage_error;

					case Asset_error::resolve_failed:
					case Asset_error::loading_failed: return Error_type::asset_not_found;
				}

				MIRRAGE_FAIL("Unexpected Asset_error: " << e);
			}
		};
		const Error_type_category error_cat{};
	} // namespace

	std::error_code make_error_code(Asset_error e) { return {static_cast<int>(e), error_cat}; }

} // namespace mirrage::asset
