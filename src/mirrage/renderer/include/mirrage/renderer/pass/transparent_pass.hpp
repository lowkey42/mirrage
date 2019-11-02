#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/graphic/streamed_buffer.hpp>


namespace mirrage::renderer {

	class Transparent_pass_factory;

	class Transparent_pass : public Render_pass {
	  public:
		using Factory = Transparent_pass_factory;

		Transparent_pass(Deferred_renderer&, ecs::Entity_manager& ecs, graphic::Render_target_2D& target);

		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Transparent"; }

	  private:
		Deferred_renderer&        _renderer;
		ecs::Entity_manager&      _ecs;
		vk::Format                _revealage_format;
		graphic::Render_target_2D _accum;
		graphic::Render_target_2D _revealage;

		vk::UniqueSampler                   _sampler;
		vk::UniqueDescriptorSetLayout       _desc_set_layout;
		graphic::DescriptorSet              _accum_descriptor_set;
		std::vector<graphic::DescriptorSet> _compose_descriptor_sets;

		vk::UniqueSampler                    _depth_sampler;
		graphic::Image_descriptor_set_layout _mip_desc_set_layout;
		std::vector<graphic::DescriptorSet>  _mip_descriptor_sets;

		std::vector<graphic::Framebuffer> _mip_framebuffers;
		graphic::Render_pass              _mip_render_pass;

		std::vector<graphic::Framebuffer> _accum_framebuffers;
		graphic::Render_pass              _accum_render_pass;

		graphic::Framebuffer _compose_framebuffer;
		graphic::Render_pass _compose_render_pass;

		graphic::Dynamic_buffer _light_uniforms;
		std::vector<char>       _light_uniforms_tmp;
	};

	class Transparent_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Transparent_pass_factory>();
		}

		auto create_pass(Deferred_renderer&,
		                 std::shared_ptr<void>,
		                 util::maybe<ecs::Entity_manager&>,
		                 Engine&,
		                 bool&) -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
