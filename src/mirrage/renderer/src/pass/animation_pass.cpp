#include <mirrage/renderer/pass/animation_pass.hpp>

#include <mirrage/renderer/animation_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>

#include <glm/gtx/string_cast.hpp>


using mirrage::ecs::components::Transform_comp;

namespace mirrage::renderer {

	namespace {
		constexpr auto shader_buffer_size = static_cast<vk::DeviceSize>(64 * 3 * 4 * int(sizeof(float)));
		constexpr auto initial_animation_capacity = static_cast<vk::DeviceSize>(16 * shader_buffer_size);
	} // namespace

	Animation_pass::Animation_pass(Deferred_renderer& r, ecs::Entity_manager& entities)
	  : Render_pass(r)
	  , _ecs(entities)
	  , _min_uniform_buffer_alignment(
	            r.device().physical_device_properties().limits.minUniformBufferOffsetAlignment)
	  , _max_pose_offset(initial_animation_capacity - shader_buffer_size)
	  , _animation_uniforms(r.device(), initial_animation_capacity, vk::BufferUsageFlagBits::eUniformBuffer)
	{
		_ecs.register_component_type<Pose_comp>();
		_ecs.register_component_type<Animation_comp>();
		_ecs.register_component_type<Simple_animation_controller_comp>();

		MIRRAGE_INVARIANT(!r.gbuffer().animation_data_layout,
		                  "More than one animation implementation active!");
		r.gbuffer().animation_data_layout =
		        r.device().create_descriptor_set_layout(vk::DescriptorSetLayoutBinding{
		                0, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex});


		auto buffers                 = _animation_uniforms.buffer_count();
		_animation_desc_sets         = util::build_vector(buffers, [&](auto) {
            return r.create_descriptor_set(*r.gbuffer().animation_data_layout, 1);
        });
		auto anim_desc_buffer_writes = util::build_vector(buffers, [&](auto i) {
			return vk::DescriptorBufferInfo{
			        _animation_uniforms.buffer(i), static_cast<vk::DeviceSize>(0), shader_buffer_size};
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

	void Animation_pass::update(util::Time time)
	{
		for(auto [controller, anim] : _ecs.list<Simple_animation_controller_comp, Animation_comp>()) {
			if(controller._next_animation.is_some()) {
				// fade requested
				auto& next = controller._next_animation.get_or_throw();

				if(controller._current_animation.is_nothing()
				   || controller._current_animation.get_or_throw().animation_id == next.animation_id) {
					// no fade required
					controller._current_animation = controller._next_animation;
					controller._next_animation    = util::nothing;
				} else if(controller._prev_animation.is_nothing()) {
					// no fade in progress => fade to new
					controller._prev_animation    = controller._current_animation;
					controller._current_animation = controller._next_animation;
					controller._next_animation    = util::nothing;
					controller._fade_time_left    = controller._fade_time;
				}
			}

			if(controller._fade_time_left > 0.f) {
				// fade in progress
				controller._fade_time_left = controller._fade_time_left - time.value();
				if(controller._fade_time_left <= 0.f) {
					controller._fade_time_left = 0.f;
					controller._prev_animation = util::nothing;
				}
			}

			for(auto& s : anim._animation_states) {
				if(controller._prev_animation.is_some()
				   && s.animation_id == controller._prev_animation.get_or_throw().animation_id)
					controller._prev_animation.get_or_throw().time = s.time;

				if(controller._current_animation.is_some()
				   && s.animation_id == controller._current_animation.get_or_throw().animation_id)
					controller._current_animation.get_or_throw().time = s.time;
			}

			if(controller._current_animation.is_some() && controller._prev_animation.is_some()) {
				anim._animation_states.resize(2);
				anim._animation_states[0]              = controller._prev_animation.get_or_throw();
				anim._animation_states[0].blend_weight = controller._fade_time_left / controller._fade_time;
				anim._animation_states[1]              = controller._current_animation.get_or_throw();
				anim._animation_states[1].blend_weight =
				        1.f - controller._fade_time_left / controller._fade_time;

			} else if(controller._current_animation.is_some()) {
				anim._animation_states.resize(1);
				anim._animation_states[0]              = controller._current_animation.get_or_throw();
				anim._animation_states[0].blend_weight = 1.f;
			}
		}

		for(auto& anim : _ecs.list<Animation_comp>()) {
			anim.step_time(time);
		}
	}

	void Animation_pass::pre_draw(Frame_data&)
	{
		// clear last update queues
		auto last_pose_offset = _next_pose_offset.load();
		if(last_pose_offset > _max_pose_offset)
			_max_pose_offset = std::max(std::int32_t(_max_pose_offset * 1.5f), last_pose_offset);
		_next_pose_offset = 0;

		// add padding to allow overstepping
		auto uniform_buffer_size = _max_pose_offset + gsl::narrow<std::uint32_t>(shader_buffer_size);

		if(_animation_uniforms.resize(uniform_buffer_size)) {
			// recreate DescriptorSet if the buffer has been recreated
			auto anim_desc_buffer_write = vk::DescriptorBufferInfo{
			        _animation_uniforms.write_buffer(), 0, vk::DeviceSize(64u * 3u * 4u * sizeof(float))};
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

		_animation_uniform_mem = _animation_uniforms.write_buffer_mem();

		// tell the rest of the renderer what our next uniform buffer will be
		_renderer.gbuffer().animation_data =
		        *_animation_desc_sets.at(std::size_t(_animation_uniforms.write_buffer_index()));
	}

	auto Animation_pass::_add_pose(ecs::Entity_facet entity, const Model& model)
	        -> util::maybe<std::pair<Skinning_type, std::uint32_t>>
	{
		auto pose_comp = entity.get<Pose_comp>();
		if(pose_comp.is_nothing())
			return util::nothing;

		auto& pose = pose_comp.get_or_throw();

		// update animations
		entity.get<Animation_comp>().process([&](auto& anim) {
			if(!anim._animation_states.empty() && anim._dirty)
				_update_animation(entity.handle(), anim, pose);
		});

		// TODO: add Animation_listeners to intercept/replace computed poses before drawing

		// upload pose
		auto size = std::int32_t(model.bone_count() * std::int32_t(sizeof(Final_bone_transform)));
		if(size < _min_uniform_buffer_alignment)
			size = _min_uniform_buffer_alignment;
		else
			size = size + (_min_uniform_buffer_alignment - size % _min_uniform_buffer_alignment);

		auto offset = _next_pose_offset.fetch_add(size, std::memory_order_relaxed);
		if(offset <= _max_pose_offset) {
			auto geo_matrices = gsl::span<Final_bone_transform>(
			        reinterpret_cast<Final_bone_transform*>(_animation_uniform_mem.data() + offset),
			        model.bone_count());
			pose.skeleton().to_final_transforms(pose.bone_transforms(), geo_matrices);

			return std::make_pair(pose.skeleton().skinning_type(), std::uint32_t(offset));

		} else {
			return util::nothing;
		}
	}

	void Animation_pass::post_draw(Frame_data& frame)
	{
		auto _ = _mark_subpass(frame);
		_upload_poses(frame);
	}

	void Animation_pass::_upload_poses(Frame_data& frame)
	{
		_animation_uniforms.flush(frame.main_command_buffer,
		                          vk::PipelineStageFlagBits::eVertexShader,
		                          vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eShaderRead);
	}

	void Animation_pass::_update_animation(ecs::Entity_handle owner,
	                                       Animation_comp&    anim_comp,
	                                       Pose_comp&         result)
	{
		const auto& skeleton_data = *result._skeleton;
		const auto  bone_count    = skeleton_data.bone_count();

		result._bone_transforms.resize(std::size_t(bone_count));
		std::memset(result._bone_transforms.data(), 0, sizeof(Local_bone_transform) * bone_count);

		anim_comp._dirty = false;

		for(auto& state : anim_comp._animation_states) {
			state.animation->bone_transforms(
			        state.time, state.blend_weight, state.frame_idx_hint, result._bone_transforms);
		}
	}


	auto Animation_pass_factory::create_pass(Deferred_renderer& renderer,
	                                         std::shared_ptr<void>,
	                                         util::maybe<ecs::Entity_manager&> entities,
	                                         Engine&,
	                                         bool&) -> std::unique_ptr<Render_pass>
	{
		return std::make_unique<Animation_pass>(
		        renderer, entities.get_or_throw("Animation_pass requires an entitymanager."));
	}

	auto Animation_pass_factory::rank_device(vk::PhysicalDevice,
	                                         util::maybe<std::uint32_t>,
	                                         int current_score) -> int
	{
		return current_score;
	}

	void Animation_pass_factory::configure_device(vk::PhysicalDevice,
	                                              util::maybe<std::uint32_t>,
	                                              graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
