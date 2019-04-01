#include <mirrage/renderer/pass/taa_pass.hpp>

#include <mirrage/graphic/window.hpp>

#include <glm/gtx/string_cast.hpp>


namespace mirrage::renderer {

	using namespace util::unit_literals;

	namespace {
		auto build_render_pass(Deferred_renderer&         renderer,
		                       vk::DescriptorSetLayout    desc_set_layout,
		                       graphic::Render_target_2D& write_tex,
		                       graphic::Framebuffer&      out_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto screen = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.gbuffer().color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto pipeline                    = graphic::Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample             = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending          = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("pcs"_strid,
			                           sizeof(Taa_constants),
			                           vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex);

			auto& pass = builder.add_subpass(pipeline).color_attachment(screen);

			pass.stage("taa"_strid)
			        .shader("frag_shader:taa"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:taa"_aid, graphic::Shader_stage::vertex);

			auto render_pass = builder.build();

			out_framebuffer = builder.build_framebuffer(
			        {write_tex.view(0), util::Rgba{}}, write_tex.width(), write_tex.height());

			return render_pass;
		}

		constexpr float halton_seq(int prime, int index = 1)
		{
			float r = 0.0f;
			float f = 1.0f;
			int   i = index;
			while(i > 0) {
				f /= prime;
				r += f * (i % prime);
				i = static_cast<int>(i / static_cast<float>(prime));
			}

			return r;
		}
		constexpr float sample_point(int prime, int index) { return halton_seq(prime, index + 1) - 0.5f; }

		template <class Function, std::size_t... Indices>
		constexpr auto make_array_helper(Function f, std::index_sequence<Indices...>)
		        -> std::array<typename std::result_of<Function(std::size_t)>::type, sizeof...(Indices)>
		{
			return {{f(Indices)...}};
		}

		template <int N, class Function>
		constexpr auto make_array(Function f)
		        -> std::array<typename std::result_of<Function(std::size_t)>::type, N>
		{
			return make_array_helper(f, std::make_index_sequence<N>{});
		}

		constexpr auto calc_halton_avg(int prime, int num_points)
		{
			auto avg = 0.f;
			for(auto i = 0; i < num_points; i++)
				avg += sample_point(prime, i);

			return avg / num_points;
		}

		template <std::size_t Size>
		constexpr auto build_halton_2_3()
		{
			// + correction offsets, so the squence is centered around (0,0)
			return make_array<Size * 2>([](std::size_t i) {
				if(i % 2 == 0)
					return sample_point(2, int(i / 2)) - calc_halton_avg(2, Size);
				else
					return sample_point(3, int(i / 2)) - calc_halton_avg(3, Size);
			});
		}

		constexpr auto offsets       = build_halton_2_3<8>();
		constexpr auto offset_factor = 0.06f;
	} // namespace


	Taa_pass::Taa_pass(Deferred_renderer&         renderer,
	                   graphic::Render_target_2D& write,
	                   graphic::Texture_2D&       read)
	  : _renderer(renderer)
	  , _sampler(renderer.device().create_sampler(1,
	                                              vk::SamplerAddressMode::eClampToEdge,
	                                              vk::BorderColor::eIntOpaqueBlack,
	                                              vk::Filter::eLinear,
	                                              vk::SamplerMipmapMode::eNearest))
	  , _descriptor_set_layout(renderer.device(), *_sampler, 5)

	  , _render_pass(build_render_pass(renderer, *_descriptor_set_layout, write, _framebuffer))
	  , _read_frame(read)
	  , _write_frame(write)
	  , _prev_frame(renderer.device(),
	                {read.width(), read.height()},
	                1,
	                renderer.gbuffer().color_format,
	                vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
	                vk::ImageAspectFlagBits::eColor)

	  , _descriptor_set(_descriptor_set_layout.create_set(renderer.descriptor_pool(),
	                                                      {renderer.gbuffer().depth.view(0),
	                                                       _read_frame.view(),
	                                                       _prev_frame.view(),
	                                                       renderer.gbuffer().prev_depth.view(0)}))
	{
	}


	void Taa_pass::update(util::Time dt) { _time_acc += dt.value(); }

	void Taa_pass::draw(Frame_data& frame)
	{

		if(_first_frame) {
			_first_frame = false;

			graphic::blit_texture(frame.main_command_buffer,
			                      _read_frame,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      _prev_frame,
			                      vk::ImageLayout::eUndefined,
			                      vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		_render_pass.execute(frame.main_command_buffer, _framebuffer, [&] {
			auto descriptor_sets =
			        std::array<vk::DescriptorSet, 2>{frame.global_uniform_set, *_descriptor_set};
			_render_pass.bind_descriptor_sets(0, descriptor_sets);

			_render_pass.push_constant("pcs"_strid, _constants);

			frame.main_command_buffer.draw(3, 1, 0, 0);
		});

		graphic::blit_texture(frame.main_command_buffer,
		                      _write_frame,
		                      vk::ImageLayout::eShaderReadOnlyOptimal,
		                      vk::ImageLayout::eShaderReadOnlyOptimal,
		                      _prev_frame,
		                      vk::ImageLayout::eShaderReadOnlyOptimal,
		                      vk::ImageLayout::eShaderReadOnlyOptimal);

		_offset_idx = (_offset_idx + 2) % (offsets.size());
	}

	namespace {
		bool is_zero(float v)
		{
			constexpr auto epsilon = 0.0000001f;
			return v > -epsilon && v < epsilon;
		}
	} // namespace

	void Taa_pass::process_camera(Camera_state& cam)
	{
		// update fov and push constants
		cam.fov_vertical = cam.fov_vertical + 5.0_deg;
		cam.fov_horizontal =
		        util::Angle(2.f * std::atan(std::tan(cam.fov_vertical.value() / 2.f) * cam.aspect_ratio));
		auto new_projection =
		        glm::perspective(cam.fov_vertical.value(), cam.aspect_ratio, cam.near_plane, cam.far_plane);
		new_projection[1][1] *= -1;
		_constants.fov_reprojection = new_projection * glm::inverse(cam.projection);
		cam.projection              = new_projection;
		cam.view_projection         = cam.projection * cam.view;

		// move projection by sub pixel offset and update push constants
		auto offset = _calc_offset(cam);
		MIRRAGE_INVARIANT(is_zero(_constants.fov_reprojection[0][3])
		                          && is_zero(_constants.fov_reprojection[1][3]),
		                  "m[0][3]!=0 or m[1][3]!=0");
		_constants.fov_reprojection[0][3] = offset.x;
		_constants.fov_reprojection[1][3] = offset.y;

		if(_first_frame) {
			_prev_view_proj = cam.view_projection;
		}

		// transform current view-space point to world-space and back to prev NDC
		_constants.reprojection = glm::inverse(_constants.fov_reprojection) * _prev_view_proj * cam.inv_view;
		_prev_view_proj         = cam.view_projection;

		cam.projection      = glm::translate(glm::mat4(1.f), glm::vec3(-offset, 0.f)) * cam.projection;
		cam.view_projection = cam.projection * cam.view;
	}
	auto Taa_pass::_calc_offset(const Camera_state& cam) const -> glm::vec2
	{
		auto offset = glm::vec2{offsets[_offset_idx], offsets[_offset_idx + 1]} * offset_factor;

		float texelSizeX = 1.f / (0.5f * cam.viewport.z - cam.viewport.x);
		float texelSizeY = 1.f / (0.5f * cam.viewport.w - cam.viewport.y);

		return offset * glm::vec2(texelSizeX, texelSizeY);
	}



	auto Taa_pass_factory::create_pass(Deferred_renderer& renderer,
	                                   std::shared_ptr<void>,
	                                   util::maybe<ecs::Entity_manager&>,
	                                   Engine&,
	                                   bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass>
	{
		if(!renderer.settings().taa)
			return {};

		auto& write = write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		auto& read = !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		write_first_pp_buffer = !write_first_pp_buffer;

		return std::make_unique<Taa_pass>(renderer, write, read);
	}

	auto Taa_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Taa_pass_factory::configure_device(vk::PhysicalDevice,
	                                        util::maybe<std::uint32_t>,
	                                        graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
