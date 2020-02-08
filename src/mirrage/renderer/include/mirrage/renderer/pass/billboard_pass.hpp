#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/entity_set_view.hpp>
#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Billboard_pass_factory;

	class Billboard_pass : public Render_pass {
	  public:
		using Factory = Billboard_pass_factory;

		Billboard_pass(Deferred_renderer&, ecs::Entity_manager&, graphic::Render_target_2D& target);

		void pre_draw(Frame_data&);
		void handle_obj(Frame_data&, Culling_mask, Billboard&, glm::vec3 pos = {});
		void post_draw(Frame_data&);

		template <typename... Passes>
		void on_draw(Frame_data& fd, Object_router<Passes...>& router);

		void update(util::Time dt) override;

		auto name() const noexcept -> const char* override { return "Billboard"; }

	  private:
		ecs::Entity_manager& _entities;
		graphic::Framebuffer _framebuffer;
		graphic::Render_pass _render_pass;

		std::vector<Billboard> _queue;
	};

	class Billboard_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Billboard_pass_factory>();
		}

		auto create_pass(Deferred_renderer&,
		                 std::shared_ptr<void>,
		                 util::maybe<ecs::Entity_manager&>,
		                 Engine&,
		                 bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue, int current_score)
		        -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t> graphics_queue,
		                      graphic::Device_create_info&) override;
	};


	template <typename... Passes>
	void Billboard_pass::on_draw(Frame_data& fd, Object_router<Passes...>& router)
	{
		using mirrage::ecs::components::Transform_comp;

		for(auto& [billboard_comp, transform] : _entities.list<Billboard_comp, Transform_comp>()) {
			for(auto&& bb : billboard_comp.billboards) {
				if(bb.active && bb.material.ready()) {
					if(bb.absolute_screen_space) {
						router.process_always_visible_obj(~Culling_mask(0), bb);

					} else {
						auto pos = transform.position + bb.offset;
						router.process_obj(pos, glm::length(bb.size), true, bb, pos);
					}
				}
			}
		}
	}

} // namespace mirrage::renderer
