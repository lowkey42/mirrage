#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/graphic/render_pass.hpp>

#include <array>


namespace mirrage::renderer {

	class Voxelization_pass : public Pass {
	  public:
		Voxelization_pass(Deferred_renderer&, ecs::Entity_manager& entities);

		void update(util::Time dt) override;
		void draw(vk::CommandBuffer&,
		          Command_buffer_source&,
		          vk::DescriptorSet global_uniform_set,
		          std::size_t       swapchain_image) override;

		auto name() const noexcept -> const char* override { return "Voxelization"; }

	  private:
		Deferred_renderer&                   _renderer;
		vk::UniqueSampler                    _sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;
		vk::Format                           _data_format;
		graphic::Render_target_2D_array      _voxel_data;
		graphic::Framebuffer                 _framebuffer;
		graphic::Render_pass                 _render_pass;
		Model_comp::Pool&                    _models;
		asset::Ptr<graphic::Texture_1D>      _mask_texture;
		graphic::DescriptorSet               _descriptor_set;
	};

	class Voxelization_pass_factory : public Pass_factory {
	  public:
		auto create_pass(Deferred_renderer&,
		                 ecs::Entity_manager&,
		                 util::maybe<Meta_system&>,
		                 bool& write_first_pp_buffer) -> std::unique_ptr<Pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue, int current_score)
		        -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t> graphics_queue,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
