#include <mirrage/utils/messagebus.hpp>

#include <mirrage/utils/random_uuid_generator.hpp>

namespace mirrage::util {
	
	auto create_request_id() -> Request_id {
		static thread_local auto generator = random_uuid_generator{};
		return generator.generate_id();
	}
	
}
