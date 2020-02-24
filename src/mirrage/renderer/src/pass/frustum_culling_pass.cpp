#include <mirrage/renderer/pass/frustum_culling_pass.hpp>

#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>

#include <glm/gtc/matrix_access.hpp>


using mirrage::ecs::components::Transform_comp;

namespace mirrage::renderer {

	Frustum_culling_pass::Frustum_culling_pass(Deferred_renderer& renderer, ecs::Entity_manager& entities)
	  : Render_pass(renderer), _ecs(entities)
	{
	}


	auto Frustum_culling_pass_factory::create_pass(Deferred_renderer& renderer,
	                                               std::shared_ptr<void>,
	                                               util::maybe<ecs::Entity_manager&> entities,
	                                               Engine&,
	                                               bool&) -> std::unique_ptr<Render_pass>
	{
		return std::make_unique<Frustum_culling_pass>(
		        renderer, entities.get_or_throw("Frustum_culling_pass requires an entitymanager."));
	}

	auto Frustum_culling_pass_factory::rank_device(vk::PhysicalDevice,
	                                               util::maybe<std::uint32_t>,
	                                               int current_score) -> int
	{
		return current_score;
	}

	void Frustum_culling_pass_factory::configure_device(vk::PhysicalDevice,
	                                                    util::maybe<std::uint32_t>,
	                                                    graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
