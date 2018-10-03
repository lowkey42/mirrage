#include <mirrage/renderer/pass/animation_pass.hpp>

#include <mirrage/renderer/animation_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>

#include <glm/gtx/string_cast.hpp>


using mirrage::ecs::components::Transform_comp;

namespace mirrage::renderer {

	namespace {
		constexpr auto shader_buffer_size         = 64 * 3 * 4 * int(sizeof(float));
		constexpr auto initial_animation_capacity = 16 * shader_buffer_size;

		auto animation_substance(util::Str_id substance_id, Skinning_type st)
		{
			switch(st) {
				case Skinning_type::linear_blend_skinning: return substance_id;
				case Skinning_type::dual_quaternion_skinning: return "dq_"_strid + substance_id;
			}
			return substance_id;
		}
	} // namespace

	Animation_pass::Animation_pass(Deferred_renderer& r, ecs::Entity_manager& entities)
	  : _renderer(r)
	  , _ecs(entities)
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
			return vk::DescriptorBufferInfo{_animation_uniforms.buffer(i), 0, shader_buffer_size};
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

	void Animation_pass::draw(Frame_data& frame)
	{
		_compute_poses(frame);

		// TODO: add Animation_listeners to intercept/replace computed poses before drawing

		_upload_poses(frame);
	}
	void Animation_pass::_compute_poses(Frame_data& frame)
	{
		// mark all cached animations as unused
		_unused_animation_keys.clear();
		_unused_animation_keys.reserve(_animation_key_cache.size());
		for(auto&& [key, value] : _animation_key_cache) {
			(void) value;
			_unused_animation_keys.emplace(key);
		}

		// update visible animations
		for(auto& geo : frame.geometry_queue) {
			_ecs.get(geo.entity).process([&](ecs::Entity_facet& entity) {
				auto anim_mb = entity.get<Animation_comp>();
				if(anim_mb.is_nothing())
					return; // not animated

				auto& anim = anim_mb.get_or_throw();
				if(anim._animation_states.empty() || !anim._dirty)
					return; // no animation playing

				entity.get<Pose_comp>().process(
				        [&](auto& skeleton) { _update_animation(geo.entity, anim, skeleton); });
			});
		}

		// erase all unused animation keys from the cache
		for(auto&& key : _unused_animation_keys) {
			_animation_key_cache.erase(key);
		}
	}
	void Animation_pass::_upload_poses(Frame_data& frame)
	{
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

		for(auto& geo : frame.geometry_queue) {
			if(!geo.model->rigged())
				continue;

			auto entity_mb = _ecs.get(geo.entity);
			if(entity_mb.is_nothing())
				continue;

			auto entity = entity_mb.get_or_throw();
			auto offset = gsl::narrow<std::uint32_t>(required_size);

			auto upload_required = entity.get<Shared_pose_comp>().process(true, [&](auto& sp) {
				auto pose_offset = util::find_maybe(_animation_uniform_offsets, sp.pose_owner);
				offset           = pose_offset.get_or(offset);
				_animation_uniform_offsets.emplace(geo.entity, pose_offset.get_or(offset));

				entity = _ecs.get(sp.pose_owner).get_or_throw("Invalid entity in render queue");
				entity.get<Pose_comp>().process([&](auto& pose) {
					geo.substance_id = animation_substance(geo.substance_id, pose.skeleton().skinning_type());
				});

				if(pose_offset.is_some())
					return false;

				return true;
			});

			if(upload_required) {
				entity.get<Pose_comp>().process([&](auto& pose) {
					geo.substance_id = animation_substance(geo.substance_id, pose.skeleton().skinning_type());

					auto [ex, success] = _animation_uniform_offsets.try_emplace(entity.handle(), offset);
					offset             = ex->second;

					if(success) {
						_animation_uniform_queue.emplace_back(geo.model, pose, offset);
						required_size += aligned_byte_size(geo.model->bone_count());
					}
				});
			}

			geo.animation_uniform_offset = offset;
		}

		if(_animation_uniforms.resize(required_size)) {
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

		for(auto& upload : _animation_uniform_queue) {
			_animation_uniforms.update_objects<Final_bone_transform>(upload.uniform_offset, [&](auto out) {
				auto geo_matrices = out.subspan(0, upload.model->bone_count());
				upload.pose->skeleton().to_final_transforms(upload.pose->bone_transforms(), geo_matrices);
			});
		}
		_animation_uniforms.flush(frame.main_command_buffer,
		                          vk::PipelineStageFlagBits::eVertexShader,
		                          vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eShaderRead);

		_renderer.gbuffer().animation_data =
		        *_animation_desc_sets.at(std::size_t(_animation_uniforms.read_buffer_index()));
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
				        * animation.bone_transform(Bone_id(i), state.time, key).get_or([&] {
					          return skeleton_data.node_transform(Bone_id(i));
				          });
			}
		}
	}


	auto Animation_pass_factory::create_pass(Deferred_renderer&   renderer,
	                                         ecs::Entity_manager& entities,
	                                         Engine&,
	                                         bool&) -> std::unique_ptr<Render_pass>
	{
		return std::make_unique<Animation_pass>(renderer, entities);
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
