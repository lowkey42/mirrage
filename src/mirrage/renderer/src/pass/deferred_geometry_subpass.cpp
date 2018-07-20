#include <mirrage/renderer/pass/deferred_geometry_subpass.hpp>

#include <mirrage/renderer/animation_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>
#include <mirrage/renderer/pass/deferred_pass.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/ecs/entity_set_view.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/renderer/model.hpp>

using mirrage::ecs::components::Transform_comp;

namespace mirrage::renderer {

	namespace {
		constexpr auto initial_animation_capacity = 256 * 4 * 4 * 4;
	}


	Deferred_geometry_subpass::Deferred_geometry_subpass(Deferred_renderer& r, ecs::Entity_manager& entities)
	  : _ecs(entities)
	  , _renderer(r)
	  , _animation_desc_layout(r.device().create_descriptor_set_layout(vk::DescriptorSetLayoutBinding{
	            0, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex}))
	  , _animation_uniforms(r.device(), initial_animation_capacity, vk::BufferUsageFlagBits::eUniformBuffer)
	{
		entities.register_component_type<Model_comp>();
		entities.register_component_type<Skeleton_comp>();

		auto buffers = _animation_uniforms.buffer_count();

		_animation_desc_sets = util::build_vector(
		        buffers, [&](auto) { return r.create_descriptor_set(*_animation_desc_layout, 1); });
		auto anim_desc_buffer_writes = util::build_vector(buffers, [&](auto i) {
			return vk::DescriptorBufferInfo{_animation_uniforms.buffer(i), 0, initial_animation_capacity};
		});
		auto anim_desc_writes        = util::build_vector(buffers, [&](auto i) {
            return vk::WriteDescriptorSet{*_animation_desc_sets.at(i),
                                          0,
                                          0,
                                          1,
                                          vk::DescriptorType::eUniformBufferDynamic,
                                          nullptr,
                                          &anim_desc_buffer_writes[i]};
        });

		r.device().vk_device()->updateDescriptorSets(
		        std::uint32_t(anim_desc_writes.size()), anim_desc_writes.data(), 0, nullptr);
	}

	void Deferred_geometry_subpass::configure_pipeline(Deferred_renderer&             renderer,
	                                                   graphic::Pipeline_description& p)
	{
		p.rasterization.cullMode = vk::CullModeFlagBits::eNone;
		p.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
		p.vertex<Model_vertex>(
		        0, false, 0, &Model_vertex::position, 1, &Model_vertex::normal, 2, &Model_vertex::tex_coords);
	}
	void Deferred_geometry_subpass::configure_subpass(Deferred_renderer&, graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:model"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex);

		pass.stage("emissive"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex);

		pass.stage("alpha_test"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_geometry_subpass::configure_animation_pipeline(Deferred_renderer&             renderer,
	                                                             graphic::Pipeline_description& p)
	{

		p.rasterization.cullMode = vk::CullModeFlagBits::eNone;
		p.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
		p.add_descriptor_set_layout(*_animation_desc_layout);
		p.vertex<Model_rigged_vertex>(0,
		                              false,
		                              0,
		                              &Model_rigged_vertex::position,
		                              1,
		                              &Model_rigged_vertex::normal,
		                              2,
		                              &Model_rigged_vertex::tex_coords,
		                              3,
		                              &Model_rigged_vertex::bone_ids,
		                              4,
		                              &Model_rigged_vertex::bone_weights);
	}
	void Deferred_geometry_subpass::configure_animation_subpass(Deferred_renderer&,
	                                                            graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:model"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex);

		pass.stage("emissive"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex);

		pass.stage("alpha_test"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_geometry_subpass::update(util::Time) {}
	void Deferred_geometry_subpass::pre_draw(Frame_data& frame)
	{
		auto end = std::partition(frame.geometry_queue.begin(), frame.geometry_queue.end(), [](auto& geo) {
			return (geo.culling_mask & 1u) != 0;
		});
		auto geo_range = util::range(frame.geometry_queue.begin(), end);

		std::sort(geo_range.begin(), geo_range.end(), [&](auto& lhs, auto& rhs) {
			auto lhs_mat = &*lhs.model->sub_meshes()[lhs.sub_mesh].material;
			auto rhs_mat = &*rhs.model->sub_meshes()[rhs.sub_mesh].material;

			return std::make_tuple(lhs_mat->material_id(), lhs_mat, lhs.model)
			       < std::make_tuple(rhs_mat->material_id(), rhs_mat, rhs.model);
		});

		auto rigged_begin = std::stable_partition(
		        geo_range.begin(), geo_range.end(), [](auto& geo) { return !geo.model->rigged(); });

		_geometry_range        = util::range(geo_range.begin(), rigged_begin);
		_rigged_geometry_range = util::range(rigged_begin, geo_range.end());


		// update upload skeleton pose
		auto required_size = std::int32_t(0);
		auto alignment     = std::int32_t(
                _renderer.device().physical_device_properties().limits.minUniformBufferOffsetAlignment);

		for(auto& geo : _rigged_geometry_range) {
			geo.animation_buffer_offset = gsl::narrow<std::uint32_t>(required_size);

			auto size = geo.model->bone_count() * std::int32_t(sizeof(glm::mat4));

			// align
			size = size < alignment ? alignment : size + (alignment - size % alignment);
			required_size += size;
		}

		if(_animation_uniforms.resize(required_size)) {
			// recreation DescriptorSet if the buffer has been recreated
			auto anim_desc_buffer_write = vk::DescriptorBufferInfo{
			        _animation_uniforms.write_buffer(), 0, vk::DeviceSize(required_size)};
			auto anim_desc_writes = vk::WriteDescriptorSet{
			        *_animation_desc_sets.at(std::size_t(_animation_uniforms.write_buffer_index())),
			        0,
			        0,
			        1,
			        vk::DescriptorType::eUniformBufferDynamic,
			        nullptr,
			        &anim_desc_buffer_write};

			_renderer.device().vk_device()->updateDescriptorSets(1, &anim_desc_writes, 0, nullptr);
		}

		_animation_uniforms.update_objects<glm::mat4>(0, [&](auto uniform_matrices) {
			for(auto& geo : _rigged_geometry_range) {
				auto entity   = _ecs.get(geo.entity).get_or_throw("Invalid entity in render queue");
				auto skeleton = entity.get<Skeleton_comp>();

				auto offset       = geo.animation_buffer_offset / std::int32_t(sizeof(glm::mat4));
				auto geo_matrices = uniform_matrices.subspan(offset, geo.model->bone_count());

				if(skeleton.is_some()
				   && skeleton.get_or_throw().bone_transforms.size()
				              == std::size_t(geo.model->bone_count())) {
					auto& sm = skeleton.get_or_throw().bone_transforms;
					std::memcpy(geo_matrices.data(), sm.data(), sm.size() * sizeof(glm::mat4));

				} else {
					std::fill(geo_matrices.begin(), geo_matrices.end(), glm::mat4(1));
				}
			}
		});
		_animation_uniforms.flush(frame.main_command_buffer,
		                          vk::PipelineStageFlagBits::eVertexShader,
		                          vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eShaderRead);
	}

	void Deferred_geometry_subpass::draw(Frame_data& frame, graphic::Render_pass& render_pass)
	{
		auto _ = _renderer.profiler().push("Geometry");

		Deferred_push_constants dpc{};
		dpc.light_data.x = _renderer.settings().debug_disect;

		auto last_mat_id   = ""_strid;
		auto last_material = static_cast<const Material*>(nullptr);
		auto last_model    = static_cast<const Model*>(nullptr);

		auto prepare_draw = [&](auto& geo) {
			auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);
			auto  mat_id   = sub_mesh.material->material_id();

			if(sub_mesh.material->material_id() != last_mat_id) {
				last_mat_id = sub_mesh.material->material_id();

				if(_renderer.settings().debug_disect && (!mat_id || mat_id == "default"_strid)) {
					render_pass.set_stage("alpha_test"_strid);

				} else if(!mat_id) {
					render_pass.set_stage("default"_strid);
				} else {
					render_pass.set_stage(mat_id);
				}
			}

			if(&*sub_mesh.material != last_material) {
				last_material = &*sub_mesh.material;
				last_material->bind(render_pass);
			}

			if(geo.model != last_model) {
				last_model = geo.model;
				geo.model->bind_mesh(frame.main_command_buffer, 0);
			}

			dpc.model    = glm::toMat4(geo.orientation) * glm::scale(glm::mat4(1.f), geo.scale);
			dpc.model[3] = glm::vec4(geo.position, 1.f);
			dpc.model    = _renderer.global_uniforms().view_mat * dpc.model;
		};


		for(auto& geo : _geometry_range) {
			auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);
			prepare_draw(geo);

			render_pass.push_constant("dpc"_strid, dpc);

			frame.main_command_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		}

		// draw all animated models in a new subpass
		render_pass.next_subpass();
		last_mat_id   = ""_strid;
		last_material = static_cast<const Material*>(nullptr);
		last_model    = static_cast<const Model*>(nullptr);

		for(auto& geo : _rigged_geometry_range) {
			auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);
			prepare_draw(geo);

			render_pass.push_constant("dpc"_strid, dpc);

			render_pass.bind_descriptor_set(
			        2,
			        *_animation_desc_sets.at(std::size_t(_animation_uniforms.read_buffer_index())),
			        {&geo.animation_buffer_offset, 1u});

			frame.main_command_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		}
	}
} // namespace mirrage::renderer
