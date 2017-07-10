#include <mirrage/ecs/types.hpp>

#include <mirrage/ecs/ecs.hpp>

#include <mirrage/utils/string_utils.hpp>

#include <string>


namespace lux {
namespace ecs {

	namespace detail {
		extern Component_type id_generator() {
			static auto next_id = static_cast<Component_type>(0);
			return ++next_id;
		}
	}


	Entity_facet::Entity_facet(Entity_manager& manager, Entity_handle owner)
	    : _manager(&manager), _owner(owner) {
		INVARIANT(valid(), "Created Entity_facet for invalid entity: "<<entity_name(owner));
	}
	auto Entity_facet::valid()const noexcept -> bool {
		return _manager && _manager->validate(_owner);
	}

}
}
