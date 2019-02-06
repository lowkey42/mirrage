#include <mirrage/utils/log.hpp>

#ifdef MIRRAGE_ENABLE_BACKWARD
// TODO: remove after upstream is fixed
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma clang diagnostic ignored "-Wold-style-cast"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <backward.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif
#endif

#include <sstream>

namespace mirrage::util {
	std::string print_stacktrace()
	{
		auto str = std::stringstream{};

#ifdef MIRRAGE_ENABLE_BACKWARD
		using namespace backward;
		StackTrace st;
		st.load_here(32);
		st.skip_n_firsts(3);
		Printer p;
		p.print(st, str);
#endif

		return str.str();
	}
} // namespace mirrage::util
