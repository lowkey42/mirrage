#include <mirrage/utils/types.hpp>

#include <iostream>


namespace mirrage::util {

	namespace detail {

		std::ostream& operator<<(std::ostream& os, Out_wrapper<std::int64_t> v) { return os << v; }

		std::ostream& operator<<(std::ostream& os, Out_wrapper<std::uint64_t> v) { return os << v; }

		std::ostream& operator<<(std::ostream& os, Out_wrapper<float> v) { return os << v; }

		std::ostream& operator<<(std::ostream& os, Out_wrapper<double> v) { return os << v; }

	} // namespace detail


	std::ostream& operator<<(std::ostream& os, const Flag& v) {
		if(v)
			return os << "true";
		else
			return os << "false";
	}

} // namespace mirrage::util
