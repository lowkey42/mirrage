#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/graphic/streamed_buffer.hpp>
#include <mirrage/gui/gui.hpp>

#include <unordered_map>
#include <vector>


namespace mirrage::renderer {

	class Gui_pass_factory;

	class Gui_pass : public Render_pass, public gui::Gui_renderer_interface {
	  public:
		using Factory = Gui_pass_factory;

		Gui_pass(Deferred_renderer&, Engine&);


		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto load_texture(int width, int height, int channels, const std::uint8_t* data)
		        -> std::shared_ptr<struct nk_image> override;

		auto load_texture(const asset::AID&) -> std::shared_ptr<struct nk_image> override;

		auto name() const noexcept -> const char* override { return "GUI"; }

	  protected:
		void prepare_draw(gsl::span<const std::uint16_t>   indices,
		                  gsl::span<const gui::Gui_vertex> vertices,
		                  glm::mat4                        view_proj) override;
		void draw_elements(int           texture_handle,
		                   glm::vec4     clip_rect,
		                   std::uint32_t offset,
		                   std::uint32_t count) override;
		void finalize_draw() override;

	  private:
		struct Loaded_texture {
		  public:
			struct nk_image handle;

			auto get_if_ready() -> util::maybe<const graphic::DescriptorSet&>;

			Loaded_texture(int                  handle,
			               graphic::Texture_ptr texture,
			               vk::Sampler,
			               Deferred_renderer&,
			               vk::DescriptorSetLayout);
			~Loaded_texture();

		  private:
			graphic::DescriptorSet descriptor_set;
			graphic::Texture_ptr   texture;
			vk::Sampler            sampler;
			Deferred_renderer*     renderer;
			bool                   initialized = false;
		};

		Deferred_renderer&                _renderer;
		std::vector<graphic::Framebuffer> _framebuffers;
		vk::UniqueSampler                 _sampler;
		vk::UniqueDescriptorSetLayout     _descriptor_set_layout;
		graphic::Render_pass              _render_pass;
		graphic::DescriptorSet            _descriptor_set;
		graphic::Streamed_buffer          _mesh_buffer;

		// texture cache/store
		int                                                            _next_texture_handle = 0;
		std::vector<std::shared_ptr<Loaded_texture>>                   _loaded_textures;
		std::unordered_map<asset::AID, std::weak_ptr<struct nk_image>> _loaded_textures_by_aid;
		std::unordered_map<int, std::weak_ptr<Loaded_texture>>         _loaded_textures_by_handle;

		// temporary values used during draw
		int                                _bound_texture_handle = -1;
		util::maybe<vk::CommandBuffer>     _current_command_buffer;
		util::maybe<graphic::Framebuffer&> _current_framebuffer;
	};


	class Gui_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override { return render_pass_id_of<Gui_pass_factory>(); }

		auto requires_gbuffer() const noexcept -> bool override { return false; }

		auto create_pass(Deferred_renderer&, util::maybe<ecs::Entity_manager&>, Engine&, bool&)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
