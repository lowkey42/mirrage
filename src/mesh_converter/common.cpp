#include "common.hpp"

namespace mirrage {
	std::atomic<std::size_t> parallel_tasks_started = 0;
	std::atomic<std::size_t> parallel_tasks_done    = 0;
} // namespace mirrage
