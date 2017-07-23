#include <mirrage/renderer/pass/taa_pass.hpp>

#include <mirrage/graphic/window.hpp>

#include <glm/gtx/string_cast.hpp>


namespace mirrage {
namespace renderer {

	using namespace util::unit_literals;

	namespace {
		auto build_render_pass(Deferred_renderer& renderer,
		                       vk::DescriptorSetLayout desc_set_layout,
		                       graphic::Render_target_2D& write_tex,
		                       graphic::Framebuffer& out_framebuffer) {

			auto builder = renderer.device().create_render_pass_builder();

			auto screen = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				renderer.gbuffer().color_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});

			auto pipeline = graphic::Pipeline_description {};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("pcs"_strid, sizeof(Taa_constants),
			                           vk::ShaderStageFlagBits::eFragment|vk::ShaderStageFlagBits::eVertex);

			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(screen);

			pass.stage("taa"_strid)
			    .shader("frag_shader:taa"_aid, graphic::Shader_stage::fragment)
			    .shader("vert_shader:taa"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);
			
			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();

			out_framebuffer = builder.build_framebuffer({write_tex.view(0), util::Rgba{}},
				                                        write_tex.width(),
				                                        write_tex.height());

			return render_pass;
		}

		constexpr float halton_seq(int prime, int index = 1) {
			float r = 0.0f;
			float f = 1.0f;
			int i = index;
			while (i > 0) {
				f /= prime;
				r += f * (i % prime);
				i = static_cast<int>(i / static_cast<float>(prime));
			}

			return r;
		}

		template<class Function, std::size_t... Indices>
		constexpr auto make_array_helper(Function f, std::index_sequence<Indices...>)
		-> std::array<typename std::result_of<Function(std::size_t)>::type, sizeof...(Indices)>
		{
		    return {{ f(Indices)... }};
		}

		template<int N, class Function>
		constexpr auto make_array(Function f)
		-> std::array<typename std::result_of<Function(std::size_t)>::type, N>
		{
		    return make_array_helper(f, std::make_index_sequence<N>{});
		}

		template<std::size_t Size>
		constexpr auto build_halton_2_3() {
			return make_array<Size*2>([](std::size_t i) {
				return halton_seq(i%2==0 ? 2 : 3, i+1) - 0.5f;
			});
		}

		constexpr auto offsets = build_halton_2_3<16>();
		constexpr auto offset_factor = 0.01f;
	}


	Taa_pass::Taa_pass(Deferred_renderer& renderer,
	                   graphic::Render_target_2D& write,
	                   graphic::Texture_2D& read)
	    : _renderer(renderer)
	    , _sampler(renderer.device().create_sampler(1, vk::SamplerAddressMode::eClampToEdge,
	                                                vk::BorderColor::eIntOpaqueBlack,
	                                                vk::Filter::eLinear,
	                                                vk::SamplerMipmapMode::eNearest))
	    , _descriptor_set_layout(renderer.device(), *_sampler, 3)
	    , _render_pass(build_render_pass(renderer, *_descriptor_set_layout, write, _framebuffer))
	    , _read_frame(read)
	    , _write_frame(write)
	    , _prev_frame(renderer.device(),
	                  {read.width(), read.height()}, 1,
	                  renderer.gbuffer().color_format,
	                  vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
	                  vk::ImageAspectFlagBits::eColor)
	    , _descriptor_set(_descriptor_set_layout.create_set(renderer.descriptor_pool(),
	                                                        {renderer.gbuffer().depth.view(0),
	                                                         _read_frame.view(),
	                                                         _prev_frame.view()})) {
	}


	void Taa_pass::update(util::Time dt) {
		_time_acc += dt.value();
	}

	void Taa_pass::draw(vk::CommandBuffer& command_buffer,
	                     Command_buffer_source&,
	                     vk::DescriptorSet global_uniform_set,
	                     std::size_t) {
		
		if(_first_frame) {
			_first_frame = false;

			graphic::blit_texture(command_buffer,
			                      _read_frame,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      _prev_frame,
			                      vk::ImageLayout::eUndefined,
			                      vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		_render_pass.execute(command_buffer, _framebuffer, [&] {
			auto descriptor_sets = std::array<vk::DescriptorSet, 2> {
				global_uniform_set,
				*_descriptor_set
			};
			_render_pass.bind_descriptor_sets(0, descriptor_sets);

			_render_pass.push_constant("pcs"_strid, _constants);

			command_buffer.draw(3, 1, 0, 0);
		});
		
		graphic::blit_texture(command_buffer,
		                      _write_frame,
		                      vk::ImageLayout::eShaderReadOnlyOptimal,
		                      vk::ImageLayout::eShaderReadOnlyOptimal,
		                      _prev_frame,
		                      vk::ImageLayout::eShaderReadOnlyOptimal,
		                      vk::ImageLayout::eShaderReadOnlyOptimal);

		_offset_idx = (_offset_idx+2) % (offsets.size());
	}

	void Taa_pass::process_camera(Camera_state& cam) {
		// update fov and push constants
		cam.fov = cam.fov + 12.0_deg;
		cam.fov_vertical = util::Angle(2.f * std::atan(std::tan(cam.fov.value()/2.f) / cam.aspect_ratio));
		auto new_projection = glm::perspective(cam.fov_vertical.value(), cam.aspect_ratio,
		                                       cam.near_plane, cam.far_plane);
		new_projection[1][1] *= -1;
		_constants.fov_reprojection = new_projection * glm::inverse(cam.projection);
		cam.projection = new_projection;
		cam.view_projection = cam.projection * cam.view;

		// move projection by sub pixel offset and update push constants
		auto offset = _calc_offset(cam);
		INVARIANT(_constants.fov_reprojection[0][3]==0 && _constants.fov_reprojection[1][3]==0,
		          "m[0][3]!=0 or m[1][3]!=0");
		_constants.fov_reprojection[0][3] = offset.x;
		_constants.fov_reprojection[1][3] = offset.y;

		if(_first_frame) {
			_prev_view_proj = cam.view_projection;
		}

		// transform current view-space point to world-space and back to prev NDC
		_constants.reprojection = _prev_view_proj * cam.inv_view;
		_prev_view_proj = cam.view_projection;

		cam.projection = glm::translate(glm::mat4(), glm::vec3(-offset, 0.f)) * cam.projection;
		cam.view_projection = cam.projection * cam.view;
	}
	auto Taa_pass::_calc_offset(const Camera_state& cam)const -> glm::vec2 {
		auto offset = glm::vec2{offsets[_offset_idx], offsets[_offset_idx+1]} * offset_factor;

		float texelSizeX = 1.f / (0.5f * cam.viewport.z-cam.viewport.x);
		float texelSizeY = 1.f / (0.5f * cam.viewport.w-cam.viewport.y);

		return offset * glm::vec2(texelSizeX, texelSizeY);
	}
	


	auto Taa_pass_factory::create_pass(Deferred_renderer& renderer,
	                                   ecs::Entity_manager& entities,
	                                   util::maybe<Meta_system&> meta_system,
	                                   bool& write_first_pp_buffer) -> std::unique_ptr<Pass> {
		auto& write = write_first_pp_buffer ? renderer.gbuffer().colorA
		                                    : renderer.gbuffer().colorB;
		
		auto& read = !write_first_pp_buffer ? renderer.gbuffer().colorA
		                                    : renderer.gbuffer().colorB;
		
		write_first_pp_buffer = !write_first_pp_buffer;
		
		return std::make_unique<Taa_pass>(renderer, write, read);
	}

	auto Taa_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
	                                   int current_score) -> int {
		return current_score;
	}

	void Taa_pass_factory::configure_device(vk::PhysicalDevice,
	                                        util::maybe<std::uint32_t>,
	                                        graphic::Device_create_info&) {
	}


}
}
