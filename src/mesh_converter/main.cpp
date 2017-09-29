
#include "filesystem.hpp"
#include "model_parser.hpp"

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/stacktrace.hpp>
#include <mirrage/utils/string_utils.hpp>

#include <gsl/gsl>

#include <string>
#include <tuple>

using namespace mirrage;

auto extract_arg(std::vector<std::string>& args, const std::string& key) -> util::maybe<std::string>;

// ./mesh_converter sponza.obj
// ./mesh_converter --output=/foo/bar sponza.obj
int main(int argc, char** argv) {
	mirrage::util::init_stacktrace(argv[0]);

	if(argc < 1) {
		std::cerr << "Too few arguments!\n"
		          << "Usage ./mesh_converter [--output=DIR] INPUT [...]" << std::endl;
		return 1;
	}

	auto args   = std::vector<std::string>{const_cast<const char**>(argv + 1),
                                         const_cast<const char**>(argv + argc)};
	auto output = extract_arg(args, "--output").get_or_other("output");

	create_directory(output);
	create_directory(output + "/models");
	create_directory(output + "/materials");
	create_directory(output + "/textures");

	for(auto&& input : args) {
		convert_model(input, output);
	}
}

auto extract_arg(std::vector<std::string>& args, const std::string& key) -> util::maybe<std::string> {
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
