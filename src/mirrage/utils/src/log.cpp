#include <mirrage/utils/log.hpp>

// TODO: remove after upstream is fixed
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#include <backward.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif

#include <sstream>

namespace mirrage::util {
	std::string print_stacktrace() {
		auto str = std::stringstream{};

		using namespace backward;
		StackTrace st;
		st.load_here(32);
		st.skip_n_firsts(3);
		Printer p;
		p.print(st, str);

		return str.str();
	}
} // namespace mirrage::util
