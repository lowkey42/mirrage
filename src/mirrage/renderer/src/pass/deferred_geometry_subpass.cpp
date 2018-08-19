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
		entities.register_component_type<Pose_comp>();
		entities.register_component_type<Shared_pose_comp>();

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

		pass.stage("alphatest"_strid)
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

		pass.stage("alphatest"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex);


		pass.stage("dq_default"_strid)
		        .shader("frag_shader:model"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated_dqs"_aid, graphic::Shader_stage::vertex);

		pass.stage("dq_emissive"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated_dqs"_aid, graphic::Shader_stage::vertex);

		pass.stage("dq_alphatest"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated_dqs"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_geometry_subpass::update(util::Time) {}

	namespace {
		auto animation_substance(util::Str_id substance_id, Skinning_type st)
		{
			switch(st) {
				case Skinning_type::linear_blend_skinning: return substance_id;
				case Skinning_type::dual_quaternion_skinning: return "dq_"_strid + substance_id;
			}
			return substance_id;
		}
	} // namespace
	void Deferred_geometry_subpass::pre_draw(Frame_data& frame)
	{
		auto geo_sort = [](auto& lhs, auto& rhs) {
			auto lhs_mat = &*lhs.model->sub_meshes()[lhs.sub_mesh].material;
			auto rhs_mat = &*rhs.model->sub_meshes()[rhs.sub_mesh].material;

			return std::make_tuple(lhs.substance_id, lhs_mat, lhs.model)
			       < std::make_tuple(rhs.substance_id, rhs_mat, rhs.model);
		};

		// partition draw commands into normal and rigged geometry
		auto end = std::partition(frame.geometry_queue.begin(), frame.geometry_queue.end(), [](auto& geo) {
			return (geo.culling_mask & 1u) != 0;
		});
		auto geo_range = util::range(frame.geometry_queue.begin(), end);

		auto rigged_begin = std::partition(
		        geo_range.begin(), geo_range.end(), [](auto& geo) { return !geo.model->rigged(); });

		_geometry_range        = util::range(geo_range.begin(), rigged_begin);
		_rigged_geometry_range = util::range(rigged_begin, geo_range.end());


		// upload skeleton pose
		_animation_uniform_offsets.clear();
		_animation_uniform_queue.clear();
		auto required_size = std::int32_t(0);
		auto alignment     = std::int32_t(
                _renderer.device().physical_device_properties().limits.minUniformBufferOffsetAlignment);

		auto aligned_byte_size = [&](auto bone_count) {
			auto size = bone_count * std::int32_t(sizeof(Final_bone_transform));
			return size < alignment ? alignment : size + (alignment - size % alignment);
		};

		for(auto& geo : _rigged_geometry_range) {
			const auto offset = gsl::narrow<std::uint32_t>(required_size);

			auto entity = _ecs.get(geo.entity).get_or_throw("Invalid entity in render queue");

			auto upload_required = entity.get<Shared_pose_comp>().process(true, [&](auto& sp) {
				auto pose_offset = util::find_maybe(_animation_uniform_offsets, sp.pose_owner);
				_animation_uniform_offsets.emplace(geo.entity, pose_offset.get_or(offset));

				entity = _ecs.get(sp.pose_owner).get_or_throw("Invalid entity in render queue");
				entity.get<Pose_comp>().process([&](auto& pose) {
					geo.substance_id = animation_substance(geo.substance_id, pose.skeleton().skinning_type());
				});

				if(pose_offset.is_some())
					return false; // TODO: update geo.substance_id here, too

				return true;
			});

			if(upload_required) {
				entity.get<Pose_comp>().process([&](auto& pose) {
					geo.substance_id = animation_substance(geo.substance_id, pose.skeleton().skinning_type());

					auto [_, success] = _animation_uniform_offsets.try_emplace(entity.handle(), offset);
					(void) _;

					if(success) {
						_animation_uniform_queue.emplace_back(geo.model, pose, required_size);
						required_size += aligned_byte_size(geo.model->bone_count());
					}
				});
			}
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

		for(auto& upload : _animation_uniform_queue) {
			_animation_uniforms.update_objects<Final_bone_transform>(upload.uniform_offset, [&](auto out) {
				auto geo_matrices = out.subspan(0, upload.model->bone_count());
				upload.pose->skeleton().to_final_transforms(upload.pose->bone_transforms(), geo_matrices);
			});
		}
		_animation_uniforms.flush(frame.main_command_buffer,
		                          vk::PipelineStageFlagBits::eVertexShader,
		                          vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eShaderRead);


		// sort geometry for efficient rendering
		std::sort(_geometry_range.begin(), _geometry_range.end(), geo_sort);
		std::sort(_rigged_geometry_range.begin(), _rigged_geometry_range.end(), geo_sort);
	}

	void Deferred_geometry_subpass::draw(Frame_data& frame, graphic::Render_pass& render_pass)
	{
		auto _ = _renderer.profiler().push("Geometry");

		Deferred_push_constants dpc{};
		dpc.light_data.x = _renderer.settings().debug_disect;

		auto last_substance_id = ""_strid;
		auto last_material     = static_cast<const Material*>(nullptr);
		auto last_model        = static_cast<const Model*>(nullptr);

		auto prepare_draw = [&](auto& geo) {
			auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);

			if(geo.substance_id != last_substance_id) {
				last_substance_id = geo.substance_id;
				render_pass.set_stage(geo.substance_id);
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
		last_substance_id = ""_strid;
		last_material     = static_cast<const Material*>(nullptr);
		last_model        = static_cast<const Model*>(nullptr);

		for(auto& geo : _rigged_geometry_range) {
			auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);
			prepare_draw(geo);

			render_pass.push_constant("dpc"_strid, dpc);

			auto uniform_offset = _animation_uniform_offsets.at(geo.entity);
			render_pass.bind_descriptor_set(
			        2,
			        *_animation_desc_sets.at(std::size_t(_animation_uniforms.read_buffer_index())),
			        {&uniform_offset, 1u});

			frame.main_command_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		}
	}
} // namespace mirrage::renderer
