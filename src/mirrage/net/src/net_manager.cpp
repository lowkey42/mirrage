#include <mirrage/net/net_manager.hpp>

#include <mirrage/utils/log.hpp>

#include <enet/enet.h>

namespace mirrage::net {

	std::atomic<std::int32_t> Net_manager::use_count = 0;

	Net_manager::Net_manager() {
		if((use_count++) == 0) {
			if(enet_initialize() != 0) {
				MIRRAGE_FAIL("An error occurred while initializing ENet.");
			}
		} else {
			MIRRAGE_WARN("BUG: Multiple living Net_manager in a single application are.");
		}

		MIRRAGE_INVARIANT(use_count > 0, "Race for Net_manager construction/destruction");
	}

	Net_manager::~Net_manager() {
		if(--use_count == 0) {
			enet_deinitialize();
		}
		MIRRAGE_INVARIANT(use_count == 0, "Race for Net_manager construction/destruction");
	}

} // namespace mirrage::net
