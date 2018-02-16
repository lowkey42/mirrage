#pragma once

#include <system_error>


namespace mirrage::asset {

	enum class Asset_error {
		// low-level/physicsFS errors
		unspecified_error = 1, /**< Error not otherwise covered here.     */
		out_of_memory,         /**< Memory allocation failed.             */
		not_initialized,       /**< PhysicsFS is not initialized.         */
		already_initialized,   /**< PhysicsFS is already initialized.     */
		no_prog_name,          /**< Needed argv[0], but it is NULL.       */
		unsupported,           /**< Operation or feature unsupported.     */
		out_of_bound,          /**< Attempted to access past end of file. */
		files_still_open,      /**< Files still open.                     */
		invalid_argument,      /**< Bad parameter passed to an function.  */
		not_mounted,           /**< Requested archive/dir not mounted.    */
		not_found,             /**< File (or whatever) not found.         */
		symlink_forbidden,     /**< Symlink seen when not permitted.      */
		no_write_dir,          /**< No write dir has been specified.      */
		open_for_reading,      /**< Wrote to a file opened for reading.   */
		open_for_writing,      /**< Read from a file opened for writing.  */
		not_a_file,            /**< Needed a file, got a directory (etc). */
		read_only,             /**< Wrote to a read-only filesystem.      */
		corrupt,               /**< Corrupted data encountered.           */
		symlink_loop,          /**< Infinite symbolic link loop.          */
		io_error,              /**< i/o error (hardware failure, etc).    */
		permission_denied,     /**< Permission denied.                    */
		no_space_left,         /**< No space (disk full, over quota, etc) */
		bad_filename,          /**< Filename is bogus/insecure.           */
		busy,                  /**< Tried to modify a file the OS needs.  */
		dir_not_empty,         /**< Tried to delete dir with files in it. */
		os_error,              /**< Unspecified OS-level error.           */
		duplicate,             /**< Duplicate entry.                      */
		bad_password,          /**< Bad password.                         */
		app_callback,          /**< Application callback reported error.  */

		// asset manager error
		resolve_failed,
		loading_failed,
		stateful_loader_not_initialized
	};

	extern std::error_code make_error_code(Asset_error e);

} // namespace mirrage::asset

namespace std {
	template <>
	struct is_error_code_enum<mirrage::asset::Asset_error> : true_type {};
} // namespace std
