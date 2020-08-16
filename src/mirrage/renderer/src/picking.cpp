#include <mirrage/renderer/picking.hpp>

namespace mirrage::renderer {

	Picking::Picking(util::maybe<ecs::Entity_manager&> ecs, util::maybe<Camera_state>& active_camera)
	  : _ecs(ecs), _active_camera(active_camera)
	{
		ecs.process([](ecs::Entity_manager& e) { e.register_component_type<Pickable_radius_comp>(); });
	}

} // namespace mirrage::renderer
