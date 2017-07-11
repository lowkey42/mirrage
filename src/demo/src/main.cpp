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
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/stacktrace.hpp>

#include <SDL2/SDL.h>
#include <glm/vec2.hpp>

#include <iostream>
#include <exception>

using namespace mirrage; // import game namespace
using namespace std::string_literals;


namespace {
	std::unique_ptr<Engine> engine;

	void init_env(int argc, char** argv, char** env);
	void init_engine();
	void onFrame();
	void shutdown();
}


#ifdef main
int main(int argc, char** argv) {
	char* noEnv = nullptr;
	char** env = &noEnv;
#else
int main(int argc, char** argv, char** env) {
#endif

	init_env(argc, argv, env);

	init_engine();

	while(engine->running())
		onFrame();

	shutdown();

	return 0;
}

namespace {
	constexpr auto app_name = "BachelorProject";
	int argc;
	char** argv;
	char** env;


	void init_env(int argc, char** argv, char** env) {

		//auto testC = glm::vec2{1,2} - glm::vec2{1,1}; (void)testC;
		//INVARIANT(testC.y==(testA.y - testB.y), "XXX: "<<testC.y<<" != "<<(testA.y - testB.y));

		::argc = argc;
		::argv = argv;
		::env  = env;

		INFO("Game started from: "<<argv[0]<<"\n"
		     <<"Working dir: "<<asset::pwd()<<"\n"
		     <<"Version: "<<version_info::name<<"\n"
		     <<"Version-Hash: "<<version_info::hash<<"\n"
			 <<"Version-Date: "<<version_info::date<<"\n"
			 <<"Version-Subject: "<<version_info::subject<<"\n");

		try {
			util::init_stacktrace(argv[0]);
			mirrage::asset::setup_storage();

		} catch (const util::Error& ex) {
			CRASH_REPORT("Exception in init: "<<ex.what());
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sorry :-(", "Error in init", nullptr);
			shutdown();
			exit(1);
		}
	}

	void init_engine() {
		try {
			bool debug = false;
#ifndef NDEBUG
			debug = true;
#endif

			for(auto i=1; i<argc; i++) {
				if(argv[i]=="--debug"s) {
					debug = true;
				}
				if(argv[i]=="--no-debug"s) {
					debug = false;
				}
			}


			engine = std::make_unique<Game_engine>(app_name, 0,1, debug, argc, argv, env);

			if(argc>1 && argv[1]=="test"s)
				engine->screens().enter<Test_screen>();
			else
				engine->screens().enter<Test_screen>();

		} catch (const util::Error& ex) {
			CRASH_REPORT("Exception in init: "<<ex.what());
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sorry :-(", "Error in init", nullptr);
			shutdown();
			exit(1);
		}
	}

	void onFrame() {
		try {
			engine->on_frame();

		} catch (const util::Error& ex) {
			CRASH_REPORT("Exception in onFrame: "<<ex.what());
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sorry :-(", "Error in onFrame", nullptr);
			shutdown();
			exit(2);
		}
	}

	void shutdown() {
		try {
			engine.reset();

		} catch (const util::Error& ex) {
			CRASH_REPORT("Exception in shutdown: "<<ex.what());
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sorry :-(", "Error in shutdown", nullptr);
			exit(3);
		}
	}
}
