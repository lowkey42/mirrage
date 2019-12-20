
#define DOCTEST_CONFIG_IMPLEMENT

#include "common.hpp"
#include "filesystem.hpp"
#include "material_parser.hpp"
#include "model_parser.hpp"

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/string_utils.hpp>

#include <doctest.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Log.h>
#include <cxxopts.hpp>
#include <gsl/gsl>

#ifndef __clang_analyzer__
#include <async++.h>
#endif

#include <chrono>
#include <string>
#include <thread>
#include <tuple>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif


using namespace mirrage;

namespace {
	template <typename T = std::string>
	auto get_arg(cxxopts::ParseResult& args, const std::string& key) -> util::maybe<T>
	{
		if(args.count(key) > 0) {
			return args[key].as<T>();
		} else {
			return util::nothing;
		}
	}

	std::string pwd()
	{
		char cCurrentPath[FILENAME_MAX];

#ifdef _WIN32
		_getcwd(cCurrentPath, sizeof(cCurrentPath));
#else
		if(getcwd(cCurrentPath, sizeof(cCurrentPath)) == nullptr) {
			MIRRAGE_FAIL("getcwd with max length " << FILENAME_MAX << " failed with error code " << errno);
		}
#endif

		return cCurrentPath;
	}

	auto load_config(const util::maybe<std::string>& config_arg,
	                 const util::maybe<std::string>& out_arg,
	                 const std::string&              working_dir,
	                 const std::vector<std::string>& inputs) -> Mesh_converted_config;
} // namespace

// ./mesh_converter sponza.obj
// ./mesh_converter --output=/foo/bar sponza.obj
int main(int argc, char** argv)
{
	static auto fileAppender =
	        plog::RollingFileAppender<plog::TxtFormatter>("mesh_converter.log", 4L * 1024L, 4);
	static auto consoleAppender = plog::ColorConsoleAppender<plog::TxtFormatter>();
	plog::init(plog::debug, &fileAppender).addAppender(&consoleAppender);

	auto options_def =
	        cxxopts::Options("./mesh_converter",
	                         "Tool to convert textures (.png) and 3D models to the internal Mirrage format.");

	auto inputs = std::vector<std::string>{};

	// clang-format off
	options_def.add_options("Generel")
	        ("h,help", "Show this help message")
	        ("o,output", "The output directory", cxxopts::value<std::string>())
	        ("c,cfg", "The config file to use", cxxopts::value<std::string>())
	        ("input", "Input files", cxxopts::value<std::vector<std::string>>(inputs));

	options_def.add_options("Textures")
	        ("normal_texture", "Process texture as normal map", cxxopts::value<bool>()->default_value("false"))
	        ("material_texture", "Process texture as material map (only RG channels)", cxxopts::value<bool>()->default_value("false"));
	// clang-format on

	options_def.parse_positional({"input"});

	auto options = options_def.parse(argc, argv);

	if(options["help"].as<bool>()) {
		std::cout << options_def.help({"Generel", "Textures"}) << "\n" << std::flush;
		return 0;

	} else if(inputs.empty()) {
		LOG(plog::warning) << "No input files!\n" << options_def.help({"Generel", "Textures"});
		return 1;
	}


	auto output_arg = get_arg(options, "output");
	auto config     = load_config(get_arg(options, "cfg"), output_arg, pwd(), inputs);
	auto normal     = get_arg<bool>(options, "normal_texture").get_or(false);
	auto srgb       = !get_arg<bool>(options, "material_texture").get_or(false);

	auto output = output_arg.get_or(config.default_output_directory);

	create_directory(output);
	create_directory(output + "/models");
	create_directory(output + "/materials");
	create_directory(output + "/textures");

	for(auto&& input : inputs) {
		if(util::ends_with(input, ".png"))
			convert_texture(input, output, normal, srgb);
		else
			convert_model(input, output, config);
	}

	using namespace std::chrono_literals;

	auto last_parallel_tasks_done = std::size_t(0);
	while(parallel_tasks_started.load() > parallel_tasks_done.load()) {
		if(last_parallel_tasks_done != parallel_tasks_done.load()) {
			last_parallel_tasks_done = parallel_tasks_done.load();

			LOG(plog::info) << "Waiting for background tasks: " << last_parallel_tasks_done << "/"
			                << parallel_tasks_started.load();
		}
		std::this_thread::sleep_for(1s);
	}
}

namespace {
	auto load_config(const util::maybe<std::string>& config_arg,
	                 const util::maybe<std::string>& out_arg,
	                 const std::string&              working_dir,
	                 const std::vector<std::string>& inputs) -> Mesh_converted_config
	{
		if(config_arg.is_some()) {
			if(auto file = std::ifstream(config_arg.get_or_throw()); file)
				return sf2::deserialize_json<Mesh_converted_config>(file);

			else if(auto file = std::ifstream(config_arg.get_or_throw() + "/config.json"); file)
				return sf2::deserialize_json<Mesh_converted_config>(file);
		}

		if(out_arg.is_some()) {
			if(auto file = std::ifstream(out_arg.get_or_throw() + "/config.json"); file)
				return sf2::deserialize_json<Mesh_converted_config>(file);
		}

		if(auto file = std::ifstream(working_dir + "/config.json"); file)
			return sf2::deserialize_json<Mesh_converted_config>(file);

		for(auto& in : inputs) {
			auto dir = util::split_on_last(in, "/").first;
			if(auto file = std::ifstream(dir + "/config.json"); file)
				return sf2::deserialize_json<Mesh_converted_config>(file);
		}

		return {};
	}
} // namespace
