
#define DOCTEST_CONFIG_IMPLEMENT

#include "common.hpp"
#include "filesystem.hpp"
#include "helper/console.hpp"
#include "helper/progress.hpp"
#include "material_parser.hpp"
#include "model_parser.hpp"

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/string_utils.hpp>

#include <doctest.h>
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


using namespace mirrage;
using namespace mirrage::helper;
using namespace std::chrono_literals;

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

	auto load_config(const util::maybe<std::string>& config_arg,
	                 const util::maybe<std::string>& out_arg,
	                 const std::string&              working_dir,
	                 const std::vector<std::string>& inputs) -> Mesh_converted_config;
} // namespace

// ./mesh_converter sponza.obj
// ./mesh_converter --output=/foo/bar sponza.obj
int main(int argc, char** argv)
{
	auto options_def =
	        cxxopts::Options("./mesh_converter",
	                         "Tool to convert textures (.png) and 3D models to the internal Mirrage format.");

	auto inputs = std::vector<std::string>{};

	// clang-format off
	options_def.add_options("Generel")
	        ("h,help", "Show this help message")
	        ("ansi", "Use ANSI escape sequences for colored output and progress bars.", cxxopts::value<bool>()->default_value("true"))
	        ("o,output", "The output directory", cxxopts::value<std::string>())
	        ("c,cfg", "The config file to use", cxxopts::value<std::string>())
	        ("dxt_level", "The dxt compression level (0=fast, 4=small)", cxxopts::value<int>())
	        ("input", "Input files, optionally named with name=file", cxxopts::value<std::vector<std::string>>(inputs));

	options_def.add_options("Textures")
	        ("normal_texture", "Process texture as normal map", cxxopts::value<bool>()->default_value("false"))
	        ("material_texture", "Process texture as material map (only RG channels)", cxxopts::value<bool>()->default_value("false"));

	options_def.add_options("Models")
	        ("i,interactive", "Interactivly request paths for missing textures", cxxopts::value<bool>()->default_value("false"))
	        ("a,interactive_all", "Interactivly request all texture paths", cxxopts::value<bool>()->default_value("false"))
	        ("trim_bones", "Remove bones that are not referenced by any vertices or other bones", cxxopts::value<bool>())
	        ("only_animations", "Only convert animations and skip model, materials, ...", cxxopts::value<bool>())
	        ("skip_materials", "Don't convert materials and textures", cxxopts::value<bool>())
	        ("scale", "Global scale factor of the model (e.g. for Blender FBX export)", cxxopts::value<float>())
	        ("fit_skeleton", "Reuse the given skeleton for animation export (remove additional bones, ...)", cxxopts::value<std::string>())
	        ("guess_scale", "Tries to guess the scale factor per bone for animation retargeting (fit_skeleton)", cxxopts::value<bool>()->default_value("true"))
	        ("material_names", "Names of the model materials (in order)", cxxopts::value<std::vector<std::string>>())
	        ("print_materials", "Display informations about materials.", cxxopts::value<bool>())
	        ("print_animations", "Display informations about animations.", cxxopts::value<bool>())
	        ("animate_scale", "Allow animations to modify bone scales", cxxopts::value<bool>())
	        ("animate_translation", "Allow animations to modify bone translations", cxxopts::value<bool>())
	        ("animate_orientation", "Allow animations to modify bone orientations", cxxopts::value<bool>());
	// clang-format on

	options_def.parse_positional({"input"});
	options_def.positional_help("<input>");
	options_def.show_positional_help();

	auto options = options_def.parse(argc, argv);

	const auto ansi = get_arg<bool>(options, "ansi").get_or(true);

	auto consoleAppender = Mirrage_console_appender<plog::TxtFormatter>(ansi);
	plog::init(plog::info, &consoleAppender);

	auto progress = Progress_container(consoleAppender.mutex());
	consoleAppender.on_log([&] { progress.restart(); });

	if(options["help"].as<bool>()) {
		std::cout << util::replace(options_def.help(), "--input arg", "<input>    ") << "\n" << std::flush;
		return 0;

	} else if(inputs.empty()) {
		LOG(plog::warning) << "No input files!\n"
		                   << util::replace(options_def.help(), "--input arg", "<input>    ");
		return 1;
	}


	auto output_arg        = get_arg(options, "output");
	auto config            = load_config(get_arg(options, "cfg"), output_arg, pwd(), inputs);
	auto normal            = get_arg<bool>(options, "normal_texture").get_or(false);
	auto srgb              = !get_arg<bool>(options, "material_texture").get_or(false);
	auto dxt_level         = get_arg<int>(options, "dxt_level");
	config.trim_bones      = get_arg<bool>(options, "time_bones").get_or(config.trim_bones);
	config.only_animations = get_arg<bool>(options, "only_animations").get_or(config.only_animations);
	config.skip_materials  = get_arg<bool>(options, "skip_materials").get_or(config.skip_materials);
	config.scale           = get_arg<float>(options, "scale").get_or(config.scale);
	auto fit_skeleton      = get_arg<std::string>(options, "fit_skeleton");
	auto guess_scale       = get_arg<bool>(options, "guess_scale").get_or(true);
	config.trim_bones      = get_arg<bool>(options, "time_bones").get_or(config.trim_bones);
	auto material_names    = get_arg<std::vector<std::string>>(options, "material_names").get_or({});

	config.print_material_info = get_arg<bool>(options, "print_materials").get_or(config.print_material_info);
	config.print_animations    = get_arg<bool>(options, "print_animations").get_or(config.print_animations);
	config.animate_scale       = get_arg<bool>(options, "animate_scale").get_or(config.animate_scale);
	config.animate_translation =
	        get_arg<bool>(options, "animate_translation").get_or(config.animate_translation);
	config.animate_orientation =
	        get_arg<bool>(options, "animate_orientation").get_or(config.animate_orientation);

	auto interactive     = get_arg<bool>(options, "interactive").get_or(false);
	auto interactive_all = get_arg<bool>(options, "interactive_all").get_or(false);
	interactive |= interactive_all;

	dxt_level.process([&](auto l) { config.dxt_level = l; });

	auto output = output_arg.get_or(config.default_output_directory);

	create_directory(output);
	create_directory(output + "/models");
	create_directory(output + "/materials");
	create_directory(output + "/textures");

	for(auto&& input : inputs) {
		auto del  = input.find_last_of("=");
		auto name = util::maybe<std::string>::nothing();
		if(del != std::string::npos) {
			name  = input.substr(0, del);
			input = input.substr(del + 1);
		}

		if(util::ends_with(input, ".png"))
			convert_texture(input, name, output, normal, srgb, config.dxt_level, progress);
		else
			convert_model(input,
			              name,
			              output,
			              config,
			              fit_skeleton,
			              guess_scale,
			              material_names,
			              progress,
			              ansi,
			              interactive,
			              interactive_all);
	}

	if(ansi) {
		progress.enable();
	} else {
		LOG(plog::info) << "Waiting for parallel tasks...";
	}

	while(parallel_tasks_started.load() > parallel_tasks_done.load()) {
		std::this_thread::sleep_for(1s);
	}

	LOG(plog::info) << "Done";
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
