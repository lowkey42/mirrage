/** application's entry point ************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#include "game_engine.hpp"
#include "test_screen.hpp"

#include <mirrage/info.hpp>

#include <mirrage/asset/asset_manager.hpp>

#include <SDL2/SDL.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Log.h>
#include <glm/vec2.hpp>

#include <exception>
#include <iostream>

using namespace mirrage; // import game namespace
using namespace std::string_literals;


namespace {
	std::unique_ptr<Engine> engine;

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

	init_env(argc, argv, env);

	init_engine();

	while(engine->running())
		onFrame();

	shutdown();

	return 0;
}

namespace {
	constexpr auto org_name = "secondsystem";
	constexpr auto app_name = "Mirrage";
	int            argc;
	char**         argv;
	char**         env;


	void init_env(int argc, char** argv, char** env)
	{
		auto write_dir = asset::write_dir(argv[0], org_name, app_name);

		static auto fileAppender = plog::RollingFileAppender<plog::TxtFormatter>(
		        (write_dir + "/mirrage.log").c_str(), 1024L * 1024L, 4);
		static auto consoleAppender = plog::ColorConsoleAppender<plog::TxtFormatter>();
		plog::init(plog::debug, &fileAppender).addAppender(&consoleAppender);


		::argc = argc;
		::argv = argv;
		::env  = env;

		LOG(plog::debug) << "Game started from: " << argv[0] << "\n"
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


		engine = std::make_unique<Game_engine>(org_name, app_name, 0, 1, debug, argc, argv, env);

		if(argc > 1 && argv[1] == "test"s)
			engine->screens().enter<Test_screen>();
		else
			engine->screens().enter<Test_screen>();
	}

	void onFrame() { engine->on_frame(); }

	void shutdown() { engine.reset(); }
} // namespace
