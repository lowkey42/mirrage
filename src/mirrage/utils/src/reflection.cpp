#include <mirrage/utils/reflection.hpp>

#if defined(__GNUG__) && !defined(EMSCRIPTEN)
#include <memory>
#include <cxxabi.h>
#include <cstdlib>

namespace lux {
namespace util {

	std::string demangle(const char* name) {
		int status = -1;

		std::unique_ptr<char, void(*)(void*)> res {
			abi::__cxa_demangle(name, NULL, NULL, &status),
			&std::free
		};

		return status==0 ? res.get() : name;
	}

}
}

#else
namespace lux {
namespace util {
	std::string demangle(const char* name) {
		return name;
	}
}
}
#endif
