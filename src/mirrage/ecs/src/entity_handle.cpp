#include <mirrage/ecs/entity_handle.hpp>

#include <mirrage/ecs/ecs.hpp>

#include <mirrage/utils/string_utils.hpp>

#include <string>


namespace lux {
namespace ecs {

	auto get_entity_id(Entity_handle h, Entity_manager& manager) -> Entity_id {
		if(manager.validate(h)) {
			return h.id();
		} else {
			return invalid_entity_id;
		}
	}

	auto entity_name(Entity_handle h) -> std::string {
		return util::to_string(h.id()) + ":" + util::to_string<int>(h.revision());
	}

}
}
