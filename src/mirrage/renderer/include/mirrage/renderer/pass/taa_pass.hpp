#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace lux {
namespace renderer {

	struct Taa_constants {
		glm::mat4 reprojection{};
		glm::mat4 fov_reprojection{}; // fov_reprojection[3].xy = offset
	};

	class Taa_pass : public Pass {
		public:
			Taa_pass(Deferred_renderer&,
			         graphic::Render_target_2D& write,
			         graphic::Texture_2D& read);


			void update(util::Time dt) override;
			void draw(vk::CommandBuffer&, Command_buffer_source&,
			          vk::DescriptorSet global_uniform_set, std::size_t swapchain_image) override;

			void process_camera(Camera_state&) override;

			auto name()const noexcept -> const char* override {return "TAA";}
			
		private:
			Deferred_renderer&                   _renderer;
			graphic::Framebuffer                 _framebuffer;
			vk::UniqueSampler                    _sampler;
			graphic::Image_descriptor_set_layout _descriptor_set_layout;
			graphic::Render_pass                 _render_pass;
			graphic::Texture_2D&                 _read_frame;
			graphic::Render_target_2D&           _write_frame;
			graphic::Render_target_2D            _prev_frame;
			vk::UniqueDescriptorSet              _descriptor_set;

			bool          _first_frame = true;
			std::size_t   _offset_idx = 0;
			float         _time_acc = 0.f;
			glm::mat4     _prev_view_proj;
			Taa_constants _constants;

			auto _calc_offset(const Camera_state&)const -> glm::vec2;
	};

	class Taa_pass_factory : public Pass_factory {
		public:
			auto create_pass(Deferred_renderer&,
			                 ecs::Entity_manager&,
			                 util::maybe<Meta_system&>,
			                 bool& write_first_pp_buffer) -> std::unique_ptr<Pass> override;

			auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
			                 int current_score) -> int override;

			void configure_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
			                      graphic::Device_create_info&) override;
	};

}
}
