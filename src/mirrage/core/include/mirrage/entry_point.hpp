/** provides basic boilerplate code for main function ************************
 *                                                                           *
 * Copyright (c) 2020 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/info.hpp>
#include <mirrage/utils/maybe.hpp>

namespace mirrage {

	namespace detail {
		inline auto get_base_dir(bool project_based) -> util::maybe<std::string>
		{
#ifndef NDEBUG
			return (project_based ? mirrage::version_info::project_root : mirrage::version_info::engine_root)
			       + "/assets";
#else
			return util::nothing;
#endif
		}

		inline auto is_debug_mode(int argc, char** argv, char** env) -> bool
		{
			bool debug = false;
#ifndef NDEBUG
			debug = true;
#endif

			for(auto i = 1; i < argc; i++) {
				if(std::strcmp(argv[i], "--debug") == 0) {
					debug = true;
				}
				if(std::strcmp(argv[i], "--no-debug") == 0) {
					debug = false;
				}
			}

			return debug;
		}

		extern auto init_logging(int                             argc,
		                         char**                          argv,
		                         char**                          env,
		                         std::uint32_t                   version_major,
		                         std::uint32_t                   version_minor,
		                         const std::string&              org_name,
		                         const std::string&              app_name,
		                         const util::maybe<std::string>& base_dir) -> bool;
	} // namespace detail

	template <typename Engine>
	auto entry_point(int                argc,
	                 char**             argv,
	                 char**             env,
	                 std::uint32_t      version_major,
	                 std::uint32_t      version_minor,
	                 const std::string& org_name,
	                 const std::string& app_name,
	                 bool               project_based = true) -> std::unique_ptr<Engine>
	{
		auto base_dir = detail::get_base_dir(project_based);
		if(!detail::init_logging(argc, argv, env, version_major, version_minor, org_name, app_name, base_dir))
			return {};

		return std::make_unique<Engine>(org_name,
		                                app_name,
		                                base_dir,
		                                version_major,
		                                version_minor,
		                                detail::is_debug_mode(argc, argv, env),
		                                argc,
		                                argv,
		                                env);
	}

} // namespace mirrage
