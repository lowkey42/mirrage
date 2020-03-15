/** provides basic boilerplate code for main function ************************
 *                                                                           *
 * Copyright (c) 2020 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#define DOCTEST_CONFIG_IMPLEMENT

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/gui/debug_ui.hpp>
#include <mirrage/info.hpp>
#include <mirrage/utils/maybe.hpp>

#include <doctest.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Log.h>


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

		inline auto init_logging(int                             argc,
		                         char**                          argv,
		                         char**                          env,
		                         std::uint32_t                   version_major,
		                         std::uint32_t                   version_minor,
		                         const std::string&              org_name,
		                         const std::string&              app_name,
		                         const util::maybe<std::string>& base_dir) -> bool
		{
			doctest::Context context;
			context.setOption("no-run", true);
			context.applyCommandLine(argc, argv);
			context.run();

			if(context.shouldExit())
				return false;

			const auto write_dir = asset::write_dir(argv[0], org_name, app_name, base_dir);

			static auto fileAppender = plog::RollingFileAppender<plog::TxtFormatter>(
			        (write_dir + "/mirrage.log").c_str(), 1024L * 1024L, 4);
			static auto consoleAppender = plog::ColorConsoleAppender<plog::TxtFormatter>();
			plog::init(plog::debug, &fileAppender)
			        .addAppender(&consoleAppender)
			        .addAppender(&gui::debug_console_appender());

			LOG(plog::debug) << "\n"
			                 << app_name << " by " << org_name << " V" << version_major << "."
			                 << version_minor << "\n"
			                 << "Started from: " << argv[0] << "\n"
			                 << "Base dir: " << base_dir.get_ref_or("<NONE>") << "\n"
			                 << "Working dir: " << asset::pwd() << "\n"
			                 << "Write dir: " << write_dir << "\n"
			                 << "Engine Version: " << version_info::name << "\n"
			                 << "Engine Version-Hash: " << version_info::hash << "\n"
			                 << "Engine Version-Date: " << version_info::date << "\n"
			                 << "Engine Version-Subject: " << version_info::subject << "\n";

			return true;
		}
	} // namespace detail

	template <typename Engine>
	auto entry_point(int                argc,
	                 char**             argv,
	                 char**             env,
	                 std::uint32_t      version_major,
	                 std::uint32_t      version_minor,
	                 const std::string& org_name,
	                 const std::string& app_name,
	                 bool               project_based          = true,
	                 const std::string& archives_list_filename = asset::default_archives_list_filename,
	                 bool               headless               = false) -> std::unique_ptr<Engine>
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
		                                env,
		                                archives_list_filename,
		                                headless);
	}

} // namespace mirrage
