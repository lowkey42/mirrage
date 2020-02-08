#include <mirrage/renderer/pass/clear_pass.hpp>


namespace mirrage::renderer {

	Clear_pass::Clear_pass(Deferred_renderer& renderer) : Render_pass(renderer) {}


	void Clear_pass::update(util::Time) {}

	void Clear_pass::post_draw(Frame_data& frame)
	{
		auto _ = _mark_subpass(frame);

		auto image = _renderer.swapchain().get_images().at(frame.swapchain_image);
		graphic::clear_texture(frame.main_command_buffer,
		                       image,
		                       _renderer.swapchain().image_width(),
		                       _renderer.swapchain().image_height(),
		                       util::Rgba{0, 0, 0, 1},
		                       vk::ImageLayout::eUndefined,
		                       vk::ImageLayout::ePresentSrcKHR,
		                       0,
		                       1);
	}


	auto Clear_pass_factory::create_pass(Deferred_renderer& renderer,
	                                     std::shared_ptr<void>,
	                                     util::maybe<ecs::Entity_manager&>,
	                                     Engine&,
	                                     bool&) -> std::unique_ptr<Render_pass>
	{
		return std::make_unique<Clear_pass>(renderer);
	}

	auto Clear_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Clear_pass_factory::configure_device(vk::PhysicalDevice,
	                                          util::maybe<std::uint32_t>,
	                                          graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
