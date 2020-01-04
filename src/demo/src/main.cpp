/** application's entry point ************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#define DOCTEST_CONFIG_IMPLEMENT

#include "game_engine.hpp"
#include "menu_screen.hpp"
#include "test_animation_screen.hpp"
#include "test_screen.hpp"

#include <mirrage/info.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/gui/debug_ui.hpp>
#include <mirrage/utils/console_command.hpp>

#include <SDL.h>
#include <doctest.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Log.h>
#include <glm/vec2.hpp>

#include <exception>
#include <iostream>

using namespace mirrage; // import game namespace
using namespace std::string_literals;


namespace {
	std::unique_ptr<Engine>                          engine;
	std::unique_ptr<util::Console_command_container> global_commands;

	void init_env(int argc, char** argv, char** env);
	void init_engine();
	void onFrame();
	void shutdown();
} // namespace


#ifdef main
int main(int argc, char** argv)
{
	char*  noEnv = nullptr;
	char** env   = &noEnv;
#else
int main(int argc, char** argv, char** env)
{
#endif
	doctest::Context context;
	context.setOption("no-run", true);
	context.applyCommandLine(argc, argv);
	auto res = context.run();

	if(context.shouldExit())
		return res;

	init_env(argc, argv, env);

	init_engine();

	while(engine->running())
		onFrame();

	shutdown();

	return res;
}

namespace {
	constexpr auto org_name = "secondsystem";
	constexpr auto app_name = "Mirrage";
	auto           base_dir() -> util::maybe<std::string>
	{
#ifndef NDEBUG
		return mirrage::version_info::engine_root + "/assets";
#else
		return util::nothing;
#endif
	}

	int    argc;
	char** argv;
	char** env;


	void init_env(int argc, char** argv, char** env)
	{
		auto write_dir = asset::write_dir(argv[0], org_name, app_name, base_dir());

		static auto fileAppender = plog::RollingFileAppender<plog::TxtFormatter>(
		        (write_dir + "/mirrage.log").c_str(), 1024L * 1024L, 4);
		static auto consoleAppender = plog::ColorConsoleAppender<plog::TxtFormatter>();
		plog::init(plog::debug, &fileAppender)
		        .addAppender(&consoleAppender)
		        .addAppender(&gui::debug_console_appender());


		::argc = argc;
		::argv = argv;
		::env  = env;

		LOG(plog::debug) << "Game started from: " << argv[0] << "\n"
		                 << "Base dir: " << base_dir().get_ref_or("<NONE>") << "\n"
		                 << "Working dir: " << asset::pwd() << "\n"
		                 << "Write dir: " << write_dir << "\n"
		                 << "Version: " << version_info::name << "\n"
		                 << "Version-Hash: " << version_info::hash << "\n"
		                 << "Version-Date: " << version_info::date << "\n"
		                 << "Version-Subject: " << version_info::subject << "\n";
	}

	void init_engine()
	{
		bool debug = false;
#ifndef NDEBUG
		debug = true;
#endif

		for(auto i = 1; i < argc; i++) {
			if(argv[i] == "--debug"s) {
				debug = true;
			}
			if(argv[i] == "--no-debug"s) {
				debug = false;
			}
		}


		engine = std::make_unique<Game_engine>(org_name, app_name, base_dir(), 0, 1, debug, argc, argv, env);

		global_commands = std::make_unique<util::Console_command_container>();
		global_commands->add("screen.leave <count> | Pops the top <count> screens",
		                     [&](std::uint8_t depth) { engine->screens().leave(depth); });
		global_commands->add(
		        "screen.print | Prints the currently open screens (=> update+draw next, D> only draw, S> "
		        "don't update+draw)",
		        [&]() {
			        auto screen_list = engine->screens().print_stack();
			        LOG(plog::info) << "Open Screens: " << screen_list;
		        });

		global_commands->add("screen.enter.test | Enters the test screen",
		                     [&]() { engine->screens().enter<Test_screen>(); });
		global_commands->add("screen.enter.animation_test | Enters the animation test screen",
		                     [&]() { engine->screens().enter<Test_animation_screen>(); });

		if(argc > 1 && argv[1] == "test"s)
			engine->screens().enter<Test_screen>();
		else if(argc > 1 && argv[1] == "animation_test"s)
			engine->screens().enter<Test_animation_screen>();
		else if(argc > 1 && argv[1] == "menu"s)
			engine->screens().enter<Menu_screen>();
		else
			engine->screens().enter<Test_screen>();
	}

	void onFrame() { engine->on_frame(); }

	void shutdown()
	{
		global_commands.reset();
		engine.reset();
	}
} // namespace
