
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
#include <gsl/gsl>

#ifndef __clang_analyzer__
#include <async++.h>
#endif

#include <chrono>
#include <string>
#include <thread>
#include <tuple>


using namespace mirrage;

auto extract_arg(std::vector<std::string>& args, const std::string& key) -> util::maybe<std::string>;
auto load_config(const util::maybe<std::string>& config_arg,
                 const util::maybe<std::string>& out_arg,
                 const std::string&              working_dir,
                 const std::vector<std::string>& inputs) -> Mesh_converted_config;

constexpr static auto usage_str = "Usage ./mesh_converter [--output=DIR] [--cfg=CFG_FILE] INPUT...";

// ./mesh_converter sponza.obj
// ./mesh_converter --output=/foo/bar sponza.obj
int main(int argc, char** argv)
{
	static auto fileAppender =
	        plog::RollingFileAppender<plog::TxtFormatter>("mesh_converter.log", 4L * 1024L, 4);
	static auto consoleAppender = plog::ColorConsoleAppender<plog::TxtFormatter>();
	plog::init(plog::debug, &fileAppender).addAppender(&consoleAppender);

	if(argc <= 1) {
		LOG(plog::error) << "Too few arguments!\n" << usage_str;
		return 1;
	}

	auto args = std::vector<std::string>{const_cast<const char**>(argv + 1),
	                                     const_cast<const char**>(argv + argc)};

	if(args[0] == "--help" || args[0] == "-h") {
		LOG(plog::info) << usage_str;
		return 0;
	}

	auto output_arg = extract_arg(args, "--output");
	auto config_arg = extract_arg(args, "--cfg");
	auto config     = load_config(extract_arg(args, "--cfg"), output_arg, argv[0], args);

	auto output = output_arg.get_or(config.default_output_directory);

	create_directory(output);
	create_directory(output + "/models");
	create_directory(output + "/materials");
	create_directory(output + "/textures");

	for(auto&& input : args) {
		if(util::ends_with(input, ".png"))
			convert_texture(input, output);
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

auto extract_arg(std::vector<std::string>& args, const std::string& key) -> util::maybe<std::string>
{
	auto found =
	        std::find_if(args.begin(), args.end(), [&](auto& str) { return util::starts_with(str, key); });

	if(found == args.end())
		return mirrage::util::nothing;

	// found contains the key and the value
	if(util::contains(*found, '=') && found->back() != '=') {
		auto ret = *found;
		args.erase(found);

		ret.erase(0, ret.find('=') + 1); // only keep the value
		return {std::move(ret)};
	}

	// the next arg is the value
	if(found->back() != '=') {
		auto ret = *(found + 1);
		args.erase(found, found + 2);
		return ret;
	}

	auto next = found + 1;

	if(*next == "=") {
		next++;
	}

	if(next->front() == '=') {
		next->erase(0, 1);
	}

	auto ret = *next;
	args.erase(found, next + 1);

	return {std::move(ret)};
}

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
