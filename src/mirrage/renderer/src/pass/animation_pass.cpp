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
		// mark all cached animations as unused
		_unused_animation_keys.clear();
		_unused_animation_keys.reserve(_animation_key_cache.size());
		for(auto&& [key, value] : _animation_key_cache) {
			(void) value;
			_unused_animation_keys.emplace(key);
		}

		// clear last update queues
		_animation_uniform_offsets.clear();
		_animation_uniform_queue.clear();
		_next_pose_offset     = 0;
		_max_pose_offset      = util::max(_required_pose_offset, _max_pose_offset);
		_required_pose_offset = 0;

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

		// tell the rest of the renderer what our next uniform buffer will be
		_renderer.gbuffer().animation_data =
		        *_animation_desc_sets.at(std::size_t(_animation_uniforms.write_buffer_index()));
	}

	auto Animation_pass::_add_pose(ecs::Entity_facet entity, Model_comp& model)
	        -> util::maybe<std::pair<Skinning_type, std::uint32_t>>
	{
		auto pose_comp = entity.get<Pose_comp>();
		pose_comp.process([&](auto& skeleton) {
			// update animations
			entity.get<Animation_comp>().process([&](auto& anim) {
				if(!anim._animation_states.empty() && anim._dirty)
					_update_animation(entity.handle(), anim, skeleton);
			});
		});


		// prepare upload of poses
		auto offset      = util::maybe<std::uint32_t>::nothing();
		auto shared_pose = false;

		auto upload_required =
		        pose_comp.is_some() || entity.get<Shared_pose_comp>().process(true, [&](auto& sp) {
			        auto pose_offset = util::find_maybe(_animation_uniform_offsets, sp.pose_owner);
			        if(pose_offset.is_some())
				        offset = pose_offset.get_or_throw();

			        shared_pose = true;

			        // our Pose_comp is in another ~~castle~~ entity => fetch it and continue with that
			        entity    = _ecs.get(sp.pose_owner).get_or_throw("Invalid entity in render queue");
			        pose_comp = entity.get<Pose_comp>();

			        return pose_offset.is_nothing();
		        });

		if(upload_required) {
			pose_comp.process([&](auto& pose) {
				auto size = model.model()->bone_count() * std::int32_t(sizeof(Final_bone_transform));
				if(size < _min_uniform_buffer_alignment)
					size = _min_uniform_buffer_alignment;
				else
					size = size + (_min_uniform_buffer_alignment - size % _min_uniform_buffer_alignment);

				if(_next_pose_offset + size <= _max_pose_offset) {
					auto new_offset    = gsl::narrow<std::uint32_t>(_next_pose_offset);
					auto [ex, success] = _animation_uniform_offsets.try_emplace(entity.handle(), new_offset);
					if(success) {
						_next_pose_offset += size;
						_animation_uniform_queue.emplace_back(*model.model(), pose, ex->second);
						offset = new_offset;
					}

				} else if(!shared_pose) {
					_required_pose_offset += size;
				}
			});
		}

		if(offset.is_nothing())
			return util::nothing;

		return pose_comp.process([&](auto& skeleton) {
			return std::make_pair(skeleton.skeleton().skinning_type(), offset.get_or_throw());
		});
	}

	void Animation_pass::post_draw(Frame_data& frame)
	{
		auto _ = _mark_subpass(frame);

		// erase all unused animation keys from the cache
		for(auto&& key : _unused_animation_keys) {
			_animation_key_cache.erase(key);
		}

		// TODO: add Animation_listeners to intercept/replace computed poses before drawing

		_upload_poses(frame);
	}

	void Animation_pass::_upload_poses(Frame_data& frame)
	{
		// copy final pose transforms into gpu buffer
		for(auto& upload : _animation_uniform_queue) {
			_animation_uniforms.update_objects<Final_bone_transform>(upload.uniform_offset, [&](auto out) {
				auto geo_matrices = out.subspan(0, upload.model->bone_count());
				upload.pose->skeleton().to_final_transforms(upload.pose->bone_transforms(), geo_matrices);
			});
		}
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

		result._bone_transforms.clear();
		result._bone_transforms.resize(std::size_t(bone_count), {{0, 0, 0, 0}, {0, 0, 0}, {0, 0, 0}});

		anim_comp._dirty = false;

		for(const auto& state : anim_comp._animation_states) {
			const auto& animation = *state.animation;

			// look up cached keys
			auto key = detail::Animation_key_cache_key{owner, state.animation_id};
			_unused_animation_keys.erase(key);
			auto cached_keys = _animation_key_cache[key];
			cached_keys.resize(std::size_t(bone_count), Animation_key{});

			// update pose
			for(auto i : util::range(std::size_t(bone_count))) {
				auto& key = cached_keys[i];

				result._bone_transforms[i] +=
				        state.blend_weight
				        * animation.bone_transform(
				                Bone_id(i), state.time, key, skeleton_data.node_transform(Bone_id(i)));
			}
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
