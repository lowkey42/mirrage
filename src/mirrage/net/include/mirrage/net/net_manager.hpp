#pragma once

#include <atomic>
#include <cstdint>

namespace mirrage::net {

	/**
	 * @brief Initializes the network subsystem.
	 * Should be created exactly once and outlive all network operations!
	 */
	class Net_manager {
	  public:
		Net_manager();
		~Net_manager();

		Net_manager(const Net_manager&) = delete;
		Net_manager& operator=(const Net_manager&) = delete;

	  private:
		static std::atomic<std::int32_t> use_count;
	};

} // namespace mirrage::net
