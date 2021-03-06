#include <mirrage/utils/reflection.hpp>

#if defined(__GNUG__) && !defined(EMSCRIPTEN)
#include <cxxabi.h>
#include <cstdlib>
#include <memory>

namespace mirrage::util {

	std::string demangle(const char* name)
	{
		int status = -1;

		std::unique_ptr<char, void (*)(void*)> res{abi::__cxa_demangle(name, nullptr, nullptr, &status),
		                                           &std::free};

		return status == 0 ? res.get() : name;
	}
} // namespace mirrage::util

#else
namespace mirrage::util {
	std::string demangle(const char* name) { return name; }
} // namespace mirrage::util
#endif
