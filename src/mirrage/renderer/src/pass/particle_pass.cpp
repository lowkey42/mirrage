#include <mirrage/renderer/pass/particle_pass.hpp>

#include <mirrage/renderer/particle_system.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/entity_manager.hpp>
#include <mirrage/ecs/entity_set_view.hpp>
#include <mirrage/graphic/window.hpp>


namespace mirrage::renderer {

	using namespace graphic;
	using ecs::components::Transform_comp;

	namespace {

		constexpr auto spawn_workgroup_size             = 32;
		constexpr auto update_workgroup_size            = 32;
		constexpr auto initial_particle_capacity        = 128;
		constexpr auto initial_particle_type_capacity   = 16;
		constexpr auto initial_global_effector_capacity = 4;
		constexpr auto particle_size_bytes = vk::DeviceSize(2u * sizeof(glm::vec4) + 4u * sizeof(float));
		constexpr auto effector_size_bytes = vk::DeviceSize(2u * sizeof(glm::vec4) + 4u * sizeof(float));
		constexpr auto shared_uniforms_size_bytes = vk::DeviceSize(4u * sizeof(std::int32_t));

		struct Shared_uniform_data {
			std::int32_t effector_count;
			std::int32_t global_effector_count;
			std::int32_t padding[2];
			// + effector array
		};
		struct Uniform_Effector {
			glm::vec4 force_dir; // w=padding
			glm::vec4 position;  // w=padding

			float force;
			float distance_decay;
			float mass_scale; // used as a=mix(F/m, F, mass_scale)
			float negative_mass_scale;
		};
		void effector_to_uniform(const Particle_effector_config& effector,
		                         const glm::vec3&                parent_position,
		                         const glm::quat&                parent_rotation,
		                         Uniform_Effector&               out)
		{
			auto orientation = effector.absolute ? effector.rotation
			                                     : glm::normalize(parent_rotation * effector.rotation);
			out.force_dir = orientation * glm::vec4(effector.force_dir, 0.f);
			out.position  = glm::vec4(
                    effector.absolute ? effector.position : parent_position + effector.position, 1.f);
			out.force               = effector.force;
			out.distance_decay      = effector.distance_decay;
			out.mass_scale          = effector.scale_with_mass ? 1.f : 0.f;
			out.negative_mass_scale = effector.negative_mass_scale;
		}


		struct Type_uniforms {
			std::uint32_t normal_distribution_flags;
			std::uint32_t flags; // 0b1: rotate_with_velocity; 0b10: symmetric_scaling
			float         loop_keyframe_time;
			std::uint32_t keyframe_count;
			// + keyframes
		};

		struct Update_push_constants {
			float         timestep;
			std::uint32_t particle_read_offset;
			std::uint32_t particle_read_count;

			std::uint32_t effector_count;
			std::uint32_t effector_offset;
		};

		auto align(vk::DeviceSize offset, vk::DeviceSize alignment)
		{
			auto diff = offset % alignment;
			return diff == 0 ? offset : offset + (alignment - diff);
		}

	} // namespace

	void Particle_pass::Per_frame_data::reserve(Deferred_renderer& renderer,
	                                            std::int32_t       particle_count,
	                                            std::int32_t       particle_type_count,
	                                            std::int32_t       global_effector_count)
	{
		if(capacity < particle_count) {
			capacity        = particle_count + 32;
			auto size_bytes = vk::DeviceSize(capacity) * particle_size_bytes;

			auto allowed_queues =
			        std::array<uint32_t, 2>{renderer.compute_queue_family(), renderer.queue_family()};
			auto queue_count = gsl::narrow<std::uint32_t>(allowed_queues.size());
			if(allowed_queues[0] == allowed_queues[1])
				queue_count = 1;

			auto create_info = vk::BufferCreateInfo{
			        vk::BufferCreateFlags{},
			        size_bytes,
			        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer
			                | vk::BufferUsageFlagBits::eVertexBuffer,
			        queue_count > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
			        queue_count,
			        allowed_queues.data()};
			particles = renderer.device().create_buffer(create_info, false, graphic::Memory_lifetime::normal);
		}

		if(effector_capacity < global_effector_count) {
			effector_capacity = global_effector_count + 4;
			auto size_bytes =
			        vk::DeviceSize(effector_capacity) * effector_size_bytes + shared_uniforms_size_bytes;

			auto create_info = vk::BufferCreateInfo{
			        vk::BufferCreateFlags{},
			        size_bytes,
			        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer};
			shared_uniforms =
			        renderer.device().create_buffer(create_info, true, graphic::Memory_lifetime::normal);
		}

		particle_type_data.reserve(std::size_t(particle_type_count));
	}

	auto Particle_pass::Per_frame_data::next_particle_type_data() -> Update_uniform_buffer&
	{
		if(std::size_t(next_free_particle_type_data) >= particle_type_data.size()) {
			particle_type_data.emplace_back();
		}
		return particle_type_data.at(std::size_t(next_free_particle_type_data++));
	}

	void Particle_pass::Update_uniform_buffer::reserve(Deferred_renderer& renderer, std::int32_t new_capacity)
	{
		if(capacity < new_capacity) {
			capacity = new_capacity;

			auto allowed_queues =
			        std::array<uint32_t, 2>{renderer.compute_queue_family(), renderer.queue_family()};
			auto queue_count = gsl::narrow<std::uint32_t>(allowed_queues.size());
			if(allowed_queues[0] == allowed_queues[1])
				queue_count = 1;

			auto size_bytes = vk::DeviceSize(capacity) * vk::DeviceSize(sizeof(Particle_keyframe))
			                  + vk::DeviceSize(sizeof(Type_uniforms));

			auto create_info = vk::BufferCreateInfo{
			        vk::BufferCreateFlags{},
			        size_bytes,
			        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
			        queue_count > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
			        queue_count,
			        allowed_queues.data()};
			buffer = renderer.device().create_buffer(create_info, true, graphic::Memory_lifetime::normal);

			if(!desc_set)
				desc_set = renderer.create_descriptor_set(renderer.compute_storage_buffer_layout(), 1);

			// update desc_set
			auto bufferInfo = vk::DescriptorBufferInfo{*buffer, 0, VK_WHOLE_SIZE};
			auto desc_write = vk::WriteDescriptorSet{
			        *desc_set, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &bufferInfo};

			renderer.device().vk_device()->updateDescriptorSets(1u, &desc_write, 0, nullptr);
		}
	}

	Particle_pass::Particle_pass(Deferred_renderer& renderer, ecs::Entity_manager& ecs)
	  : _renderer(renderer)
	  , _ecs(ecs)
	  , _rand(util::construct_random_engine())
	  , _storage_buffer_offset_alignment(
	            renderer.device().physical_device_properties().limits.minStorageBufferOffsetAlignment)
	  , _descriptor_set_layout(create_particle_shared_desc_set_layout(renderer.device()))
	  , _pipeline_layout(create_particle_script_pipeline_layout(renderer.device(),
	                                                            *_descriptor_set_layout,
	                                                            renderer.compute_storage_buffer_layout(),
	                                                            renderer.compute_uniform_buffer_layout()))
	  , _update_fence(renderer.device().create_fence(false))
	  , _per_frame_data(util::build_vector<Per_frame_data>(
	            renderer.device().max_frames_in_flight() + 3, [&](auto, auto& vec) {
		            vec.emplace_back().reserve(renderer,
		                                       initial_particle_capacity,
		                                       initial_particle_type_capacity,
		                                       initial_global_effector_capacity);
	            }))
	{
		ecs.register_component_type<Particle_system_comp>();
	}


	void Particle_pass::update(util::Time dt)
	{
		for(auto& [transform, ps] : _ecs.list<Transform_comp, Particle_system_comp>()) {
			if(!ps.particle_system.cfg().ready())
				continue;

			ps.particle_system._last_position = ps.particle_system.position();
			ps.particle_system.position(transform.position);
			ps.particle_system.rotation(transform.orientation);
			for(auto& e : ps.particle_system.emitters())
				e.incr_time(dt.value());
		}

		_dt += dt.value();
	}

	void Particle_pass::draw(Frame_data& frame)
	{
		if(_update_submitted && _update_fence) {
			_update_fence.reset();
			_update_submitted = false;

			// invalidate old particle data
			_rev++;

			// update offsets/count from feedback buffer
			auto feedback = reinterpret_cast<const Emitter_range*>(
			        _feedback_buffer_host.memory().mapped_addr().get_or_throw(
			                "particle feedback buffer is not mapped"));

			auto frame_idx =
			        _current_frame <= 0 ? _per_frame_data.size() - 1u : std::size_t(_current_frame - 1);
			for(auto i = std::uint32_t(0); i < _emitter_gpu_data.size(); i++) {
				auto& emitter_weak = _emitter_gpu_data[i];
				if(auto emitter = emitter_weak.lock(); emitter) {
					auto offset = feedback->offset;
					auto count  = feedback->count;
					feedback++;
					emitter->set(&_rev,
					             *_per_frame_data.at(frame_idx).particles,
					             emitter->next_uniforms(),
					             offset,
					             count,
					             i);
				}
			}

			_emitter_gpu_data.clear();
		}

		if(!_update_submitted) {
			// sort for update
			std::sort(frame.particle_queue.begin(), frame.particle_queue.end(), [&](auto& lhs, auto& rhs) {
				return std::make_tuple(&*lhs.emitter->cfg().type, lhs.effectors.empty())
				       < std::make_tuple(&*lhs.emitter->cfg().type, rhs.effectors.empty());
			});

			_submit_update(frame);
		}

		// sort for draw
		std::sort(frame.particle_queue.begin(), frame.particle_queue.end(), [&](auto& lhs, auto& rhs) {
			auto lhs_draw = (lhs.culling_mask & 1) != 0 && lhs.emitter->drawable();
			auto rhs_draw = (lhs.culling_mask & 1) != 0 && rhs.emitter->drawable();
			if(lhs_draw != rhs_draw)
				return lhs_draw;

			auto& lhs_type = *lhs.emitter->cfg().type;
			auto& rhs_type = *rhs.emitter->cfg().type;

			return std::make_tuple(lhs_type.blend,
			                       lhs_type.geometry,
			                       &*lhs_type.material,
			                       lhs_type.model ? &*lhs_type.model : nullptr)
			       < std::make_tuple(rhs_type.blend,
			                         rhs_type.geometry,
			                         &*rhs_type.material,
			                         rhs_type.model ? &*rhs_type.model : nullptr);
		});
	}

	void Particle_pass::_submit_update(Frame_data& frame)
	{
		if(frame.particle_queue.empty())
			return;

		MIRRAGE_INVARIANT(_emitter_gpu_data.empty(),
		                  "_emitter_gpu_data is not empty at the start of the update process.");

		const auto max_particles = _renderer.settings().max_particles;

		auto& data = _per_frame_data.at(std::size_t(_current_frame));

		data.commands = _renderer.create_compute_command_buffer();
		auto commands = *data.commands;
		commands.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

		auto [feedback, feedback_mapping] = _alloc_feedback_buffer(frame);

		// calc new particle count and assign the new range to each emitter
		auto last_type             = static_cast<const Particle_type_config*>(nullptr);
		auto particle_type_count   = std::int32_t(0);
		auto particle_count        = std::int32_t(0);
		auto global_effector_count = _ecs.list<Particle_effector_comp>().size();
		auto effector_count        = global_effector_count;

		_emitter_gpu_data.reserve(frame.particle_queue.size());
		for(auto i = std::size_t(0); i < frame.particle_queue.size(); i++) {
			auto& p = frame.particle_queue[i];

			effector_count += gsl::narrow<std::int32_t>(p.effectors.size());

			auto spawn = p.emitter->spawn(_rand);
			if(particle_count + spawn >= max_particles) {
				spawn = util::max(0, max_particles - particle_count);
				p.emitter->override_spawn(spawn);
			}
			auto count = p.emitter->particle_count() + spawn;

			feedback[std::int32_t(i)].offset = particle_count;
			feedback[std::int32_t(i)].count  = spawn;
			if(p.emitter->particle_feedback_idx().is_some())
				feedback_mapping[p.emitter->particle_feedback_idx().get_or_throw()] = std::uint32_t(i);

			particle_count += count;

			_emitter_gpu_data.emplace_back(p.emitter->gpu_data());

			if(last_type != &*p.emitter->cfg().type) {
				last_type = &*p.emitter->cfg().type;
				particle_type_count++;
			}
		}


		// resize/create new particle_buffer
		data.reserve(_renderer, particle_count, particle_type_count, effector_count);

		// init feedback buffer
		auto mapping_offset = align(vk::DeviceSize(_feedback_buffer_size * sizeof(Emitter_range)),
		                            _storage_buffer_offset_alignment);
		auto feedback_size_bytes =
		        mapping_offset + vk::DeviceSize(_feedback_buffer_size * sizeof(std::uint32_t));
		commands.copyBuffer(
		        *_feedback_buffer_host, *_feedback_buffer, vk::BufferCopy{0, 0, feedback_size_bytes});

		// barrier feedback transfer_write -> shader_read
		commands.pipelineBarrier(
		        vk::PipelineStageFlagBits::eTransfer,
		        vk::PipelineStageFlagBits::eComputeShader,
		        vk::DependencyFlags{},
		        {},
		        vk::BufferMemoryBarrier{vk::AccessFlagBits::eTransferWrite,
		                                vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
		                                VK_QUEUE_FAMILY_IGNORED,
		                                VK_QUEUE_FAMILY_IGNORED,
		                                *_feedback_buffer,
		                                0,
		                                VK_WHOLE_SIZE},
		        {});

		// write shared_uniforms
		auto shared_uniforms_ptr =
		        reinterpret_cast<char*>(data.shared_uniforms.memory().mapped_addr().get_or_throw(
		                "particle uniform buffer is not mapped"));
		auto& shared_uniforms                 = *reinterpret_cast<Shared_uniform_data*>(shared_uniforms_ptr);
		shared_uniforms.effector_count        = std::int32_t(effector_count);
		shared_uniforms.global_effector_count = std::int32_t(global_effector_count);
		auto uniform_effectors                = gsl::span<Uniform_Effector>(
                reinterpret_cast<Uniform_Effector*>(shared_uniforms_ptr + sizeof(Shared_uniform_data)),
                effector_count);
		auto i = 0;

		for(auto [e, t] : _ecs.list<Particle_effector_comp, Transform_comp>()) {
			effector_to_uniform(e.effector, t.position, t.orientation, uniform_effectors[i++]);
		}
		for(auto& p : frame.particle_queue) {
			for(auto& e : p.effectors) {
				effector_to_uniform(e, p.system->position(), p.system->rotation(), uniform_effectors[i++]);
			}
		}

		if(_first_frame)
			_update_descriptor_set(data, util::nothing);
		else {
			auto prev_frame_idx =
			        _current_frame <= 0 ? _per_frame_data.size() - 1u : std::size_t(_current_frame - 1);
			_update_descriptor_set(data, _per_frame_data.at(prev_frame_idx));
		}


		commands.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
		                            *_pipeline_layout,
		                            0,
		                            1u,
		                            data.descriptor_set.get_ptr(),
		                            0,
		                            nullptr);

		_update_type_uniforms(frame, data);
		_dispatch_emits(frame, commands);
		_dispatch_updates(frame, commands);

		// barrier feedback shader_write -> transfer_read
		commands.pipelineBarrier(
		        vk::PipelineStageFlagBits::eComputeShader,
		        vk::PipelineStageFlagBits::eTransfer,
		        vk::DependencyFlags{},
		        {},
		        vk::BufferMemoryBarrier{vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
		                                vk::AccessFlagBits::eTransferWrite,
		                                VK_QUEUE_FAMILY_IGNORED,
		                                VK_QUEUE_FAMILY_IGNORED,
		                                *_feedback_buffer,
		                                0,
		                                VK_WHOLE_SIZE},
		        {});

		// copy feedback buffer back to host visible memory
		commands.copyBuffer(
		        *_feedback_buffer, *_feedback_buffer_host, vk::BufferCopy{0, 0, feedback_size_bytes});

		// submit to async compute queue
		commands.end();
		auto submit = vk::SubmitInfo{0, nullptr, nullptr, 1, &commands};
		_renderer.compute_queue().submit(submit, _update_fence.vk_fence());
		_update_submitted = true;

		_current_frame = (_current_frame + 1) % std::int32_t(_per_frame_data.size());
		_first_frame   = false;
		_dt            = 0.f;
	}

	auto Particle_pass::_alloc_feedback_buffer(Frame_data& frame)
	        -> std::tuple<gsl::span<Emitter_range>, gsl::span<std::uint32_t>>
	{
		auto last_feedback_buffer_size = _feedback_buffer_size;
		if(frame.particle_queue.size() > _feedback_buffer_size) {
			// resize feedback_buffer
			_feedback_buffer_size = frame.particle_queue.size() + 32u;
			auto mapping_offset   = align(vk::DeviceSize(_feedback_buffer_size * sizeof(Emitter_range)),
                                        _storage_buffer_offset_alignment);
			auto feedback_size_bytes =
			        mapping_offset + vk::DeviceSize(_feedback_buffer_size * sizeof(std::uint32_t));
			auto create_info = vk::BufferCreateInfo{vk::BufferCreateFlags{},
			                                        feedback_size_bytes,
			                                        vk::BufferUsageFlagBits::eTransferDst
			                                                | vk::BufferUsageFlagBits::eTransferSrc
			                                                | vk::BufferUsageFlagBits::eStorageBuffer};
			_feedback_buffer =
			        _renderer.device().create_buffer(create_info, false, graphic::Memory_lifetime::temporary);
			_feedback_buffer_host =
			        _renderer.device().create_buffer(create_info, true, graphic::Memory_lifetime::temporary);
		}

		auto mapping_offset = align(vk::DeviceSize(_feedback_buffer_size * sizeof(Emitter_range)),
		                            _storage_buffer_offset_alignment);

		auto feedback_ptr = reinterpret_cast<char*>(_feedback_buffer_host.memory().mapped_addr().get_or_throw(
		        "particle feedback buffer is not mapped!"));
		auto feedback     = gsl::span<Emitter_range>(reinterpret_cast<Emitter_range*>(feedback_ptr),
                                                 gsl::narrow<std::int32_t>(_feedback_buffer_size));
		auto feedback_mapping =
		        gsl::span<std::uint32_t>(reinterpret_cast<std::uint32_t*>(feedback_ptr + mapping_offset),
		                                 gsl::narrow<std::int32_t>(last_feedback_buffer_size));

		return {feedback, feedback_mapping};
	}

	void Particle_pass::_update_descriptor_set(Per_frame_data& data, util::maybe<Per_frame_data&> prev_data)
	{
		if(!data.descriptor_set)
			data.descriptor_set = _renderer.create_descriptor_set(*_descriptor_set_layout, 5);

		auto desc_writes = util::build_array<5>([&](auto i) {
			return vk::WriteDescriptorSet{*data.descriptor_set, std::uint32_t(i), 0, 1};
		});

		auto shared_uniforms          = vk::DescriptorBufferInfo{*data.shared_uniforms, 0, VK_WHOLE_SIZE};
		desc_writes[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		desc_writes[0].pBufferInfo    = &shared_uniforms;

		auto particles_old_buffer = prev_data.process(*data.particles, [](auto& d) { return *d.particles; });
		auto particles_old        = vk::DescriptorBufferInfo{particles_old_buffer, 0, VK_WHOLE_SIZE};
		desc_writes[1].descriptorType = vk::DescriptorType::eStorageBuffer;
		desc_writes[1].pBufferInfo    = &particles_old;

		auto particles_new            = vk::DescriptorBufferInfo{*data.particles, 0, VK_WHOLE_SIZE};
		desc_writes[2].descriptorType = vk::DescriptorType::eStorageBuffer;
		desc_writes[2].pBufferInfo    = &particles_new;

		auto feedback_bytes           = vk::DeviceSize(_feedback_buffer_size * sizeof(Emitter_range));
		auto feedback                 = vk::DescriptorBufferInfo{*_feedback_buffer, 0, feedback_bytes};
		desc_writes[3].descriptorType = vk::DescriptorType::eStorageBuffer;
		desc_writes[3].pBufferInfo    = &feedback;

		feedback_bytes        = align(feedback_bytes, _storage_buffer_offset_alignment);
		auto feedback_mapping = vk::DescriptorBufferInfo{*_feedback_buffer, feedback_bytes, VK_WHOLE_SIZE};
		desc_writes[4].descriptorType = vk::DescriptorType::eStorageBuffer;
		desc_writes[4].pBufferInfo    = &feedback_mapping;

		_renderer.device().vk_device()->updateDescriptorSets(
		        gsl::narrow<std::uint32_t>(desc_writes.size()), desc_writes.data(), 0, nullptr);
	}

	void Particle_pass::_update_type_uniforms(Frame_data& frame, Per_frame_data& data)
	{
		data.next_free_particle_type_data = 0;

		auto update_uniforms = [&](const Particle_type_config& cfg) {
			// update uniforms
			auto& type_data = data.next_particle_type_data();
			type_data.reserve(_renderer, std::int32_t(cfg.keyframes.size()));

			auto  uniforms_ptr = reinterpret_cast<char*>(type_data.buffer.memory().mapped_addr().get_or_throw(
                    "particle type uniform buffer is not mapped"));
			auto& uniforms     = *reinterpret_cast<Type_uniforms*>(uniforms_ptr);
			auto  keyframes    = reinterpret_cast<char*>(uniforms_ptr + sizeof(Type_uniforms));

			uniforms.keyframe_count = gsl::narrow<std::uint32_t>(cfg.keyframes.size());
			auto rotate_with_velocity =
			        cfg.rotate_with_velocity ? (cfg.geometry == Particle_geometry::billboard ? 2u : 1u) : 0u;
			uniforms.flags              = (cfg.symmetric_scaling ? 1u : 0u) << 2 | rotate_with_velocity;
			uniforms.loop_keyframe_time = cfg.loop_keyframe_time;

			auto ndf = std::uint32_t(0);
			auto i   = std::uint32_t(0);
			// clang-format off
			if(cfg.color_normal_distribution_h) ndf |= std::uint32_t(1) << (i++);
			if(cfg.color_normal_distribution_s) ndf |= std::uint32_t(1) << (i++);
			if(cfg.color_normal_distribution_v) ndf |= std::uint32_t(1) << (i++);
			if(cfg.color_normal_distribution_a) ndf |= std::uint32_t(1) << (i++);

			if(cfg.rotation_normal_distribution_x) ndf |= std::uint32_t(1) << (i++);
			if(cfg.rotation_normal_distribution_y) ndf |= std::uint32_t(1) << (i++);
			if(cfg.rotation_normal_distribution_z) ndf |= std::uint32_t(1) << (i++);

			if(cfg.size_normal_distribution_x) ndf |= std::uint32_t(1) << (i++);
			if(cfg.size_normal_distribution_y) ndf |= std::uint32_t(1) << (i++);
			if(cfg.size_normal_distribution_z) ndf |= std::uint32_t(1) << (i++);
			// clang-format on

			uniforms.normal_distribution_flags = ndf;

			static_assert(std::is_standard_layout_v<Particle_keyframe>);
			std::memcpy(keyframes, cfg.keyframes.data(), cfg.keyframes.size_in_bytes());

			return *type_data.desc_set;
		};

		auto submit_batch = [&](auto&& begin, auto&& end, const Particle_type_config& type) {
			if(begin != frame.particle_queue.end()) {
				auto desc_set = update_uniforms(type);
				for(auto& p : util::range(begin, end))
					p.emitter->gpu_data()->next_uniforms(desc_set);
			}
		};

		auto batch_begin = frame.particle_queue.end();
		auto batch_type  = static_cast<const Particle_type_config*>(nullptr);

		for(auto iter = frame.particle_queue.begin(); iter != frame.particle_queue.end(); iter++) {
			auto type = &*iter->emitter->cfg().type;

			if(type != batch_type) {
				if(batch_type)
					submit_batch(batch_begin, iter, *batch_type);

				batch_type  = type;
				batch_begin = iter;
			}
		}

		submit_batch(batch_begin, frame.particle_queue.end(), *batch_type);

		for(auto& p : frame.particle_queue) {
			MIRRAGE_INVARIANT(p.emitter->gpu_data()->next_uniforms(), "no particle uniform set");
		}
	}

	namespace {
		struct Emitter_push_constants {
			glm::vec4 parent_velocity;
			glm::vec4 position;
			glm::quat rotation_quat;

			Random_value<Particle_direction> direction;

			glm::vec2 size;

			float ttl_mean;
			float ttl_stddev;
			float velocity_mean;
			float velocity_stddev;

			std::uint32_t direction_flags;

			std::uint32_t offset;
			std::uint32_t to_spawn;
			std::uint32_t base_seed;
			std::uint32_t feedback_buffer_id;
			float         timestep;
		};
	} // namespace

	void Particle_pass::_dispatch_emits(Frame_data& frame, vk::CommandBuffer commands)
	{
		const auto dt = _dt;

		auto idx    = std::int32_t(0);
		auto offset = std::int32_t(0);

		for(auto& p : frame.particle_queue) {
			const auto to_spawn = p.emitter->particles_to_spawn();

			if(to_spawn > 0) {
				// dispatch emit WRITE to new particle_buffer and feedback_buffer
				const auto& cfg = p.emitter->cfg();
				p.emitter->cfg().emit_script->bind(commands);

				auto pcs            = Emitter_push_constants{};
				pcs.parent_velocity = glm::vec4(
				        (p.system->position() - p.system->_last_position) / dt * cfg.parent_velocity, 0.f);
				pcs.position      = glm::vec4(p.system->emitter_position(*p.emitter), 0.f);
				pcs.rotation_quat = p.system->emitter_rotation(*p.emitter);
				pcs.size          = glm::vec2(cfg.size.x, cfg.size.y);

				pcs.direction = cfg.direction;

				pcs.ttl_mean        = cfg.ttl.mean;
				pcs.ttl_stddev      = cfg.ttl.stddev;
				pcs.velocity_mean   = cfg.velocity.mean;
				pcs.velocity_stddev = cfg.velocity.stddev;

				pcs.direction_flags = std::uint32_t(cfg.independent_direction ? 1 : 0)
				                      | std::uint32_t(cfg.direction_normal_distribution ? 2 : 0);

				pcs.offset             = std::uint32_t(offset);
				pcs.to_spawn           = std::uint32_t(p.emitter->particles_to_spawn());
				pcs.base_seed          = std::uniform_int_distribution<std::uint32_t>{}(_rand);
				pcs.feedback_buffer_id = std::uint32_t(idx);
				pcs.timestep           = p.emitter->last_timestep();

				commands.pushConstants(*_pipeline_layout,
				                       vk::ShaderStageFlagBits::eCompute,
				                       0,
				                       sizeof(Emitter_push_constants),
				                       &pcs);

				auto groups = static_cast<std::uint32_t>(std::ceil(float(to_spawn) / spawn_workgroup_size));
				commands.dispatch(groups, 1, 1);
			}

			idx++;
			offset += std::uint32_t(p.emitter->particle_count() + p.emitter->particles_to_spawn());
		}
	}
	void Particle_pass::_dispatch_updates(Frame_data& frame, vk::CommandBuffer commands)
	{
		const auto dt = _dt;

		auto submit_update = [&](const Particle_type_config& cfg,
		                         std::int32_t                read_offset,
		                         std::int32_t                count,
		                         std::int32_t                effector_offset,
		                         std::int32_t                effector_count,
		                         vk::DescriptorSet           uniforms) {
			if(count > 0) {
				commands.bindDescriptorSets(
				        vk::PipelineBindPoint::eCompute, *_pipeline_layout, 1u, 1u, &uniforms, 0, nullptr);

				cfg.update_script->bind(commands);

				auto pcs                 = Update_push_constants{};
				pcs.timestep             = dt;
				pcs.effector_offset      = std::uint32_t(effector_offset);
				pcs.effector_count       = std::uint32_t(effector_count);
				pcs.particle_read_offset = std::uint32_t(read_offset);
				pcs.particle_read_count  = std::uint32_t(count);

				commands.pushConstants(*_pipeline_layout,
				                       vk::ShaderStageFlagBits::eCompute,
				                       0,
				                       sizeof(Update_push_constants),
				                       &pcs);

				// dispatch update READ from old particle_buffer APPEND to new particle_buffer and feedback_buffer
				auto groups = static_cast<std::uint32_t>(std::ceil(float(count) / update_workgroup_size));
				commands.dispatch(groups, 1, 1);
			}
		};

		auto batch_any_elem    = static_cast<Particle_emitter*>(nullptr);
		auto batch_type        = static_cast<const Particle_type_config*>(nullptr);
		auto batch_range_begin = std::numeric_limits<std::int32_t>::max();
		auto batch_range_end   = std::numeric_limits<std::int32_t>::min();

		auto submit_batch = [&] {
			auto count = batch_range_end - batch_range_begin;
			if(!batch_type || count <= 0)
				return; // no prev. particles in batch => skip

			// submit
			submit_update(
			        *batch_type, batch_range_begin, count, 0, 0, batch_any_elem->gpu_data()->next_uniforms());

			// reset
			batch_any_elem    = nullptr;
			batch_type        = nullptr;
			batch_range_begin = std::numeric_limits<std::int32_t>::max();
			batch_range_end   = std::numeric_limits<std::int32_t>::min();
		};
		auto add_to_batch = [&](auto& p) {
			auto type  = &*p.emitter->cfg().type;
			auto begin = p.emitter->particle_offset();
			auto end   = p.emitter->particle_offset() + p.emitter->particle_count();

			if(type != batch_type) {
				// start new batch
				submit_batch();
				batch_any_elem    = p.emitter;
				batch_type        = type;
				batch_range_begin = begin;
				batch_range_end   = end;
			} else if(p.emitter->particle_count() > 0) {
				batch_range_begin = util::min(batch_range_begin, begin);
				batch_range_end   = util::max(batch_range_end, end);
			}
		};

		auto effector_offset = std::int32_t(_ecs.list<Particle_effector_comp>().size());

		for(auto& p : frame.particle_queue) {
			auto& gpu_data = *p.emitter->gpu_data();
			if(!gpu_data.batch_able() || !p.effectors.empty()) {
				gpu_data.batch_able(false);

				submit_batch();

				auto effector_count = std::int32_t(p.effectors.size());
				submit_update(*p.emitter->cfg().type,
				              p.emitter->particle_offset(),
				              p.emitter->particle_count(),
				              effector_offset,
				              effector_count,
				              gpu_data.next_uniforms());
				effector_offset += effector_count;

			} else {
				add_to_batch(p);
			}
		}
		submit_batch();
	}


	auto Particle_pass_factory::create_pass(Deferred_renderer& renderer,
	                                        std::shared_ptr<void>,
	                                        util::maybe<ecs::Entity_manager&> ecs,
	                                        Engine&,
	                                        bool&) -> std::unique_ptr<Render_pass>
	{
		if(ecs.is_nothing() || !renderer.settings().particles)
			return {};

		return std::make_unique<Particle_pass>(renderer, ecs.get_or_throw());
	}

	auto Particle_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Particle_pass_factory::configure_device(vk::PhysicalDevice pd,
	                                             util::maybe<std::uint32_t>,
	                                             graphic::Device_create_info& ci)
	{
		auto features = pd.getFeatures();
		if(features.fragmentStoresAndAtomics) {
			ci.features.fragmentStoresAndAtomics = true;
		} else {
			LOG(plog::warning) << "Feature fragmentStoresAndAtomics is not supported.";
		}

		if(features.vertexPipelineStoresAndAtomics) {
			ci.features.vertexPipelineStoresAndAtomics = true;
		} else {
			LOG(plog::warning) << "Feature vertexPipelineStoresAndAtomics is not supported.";
		}
	}
} // namespace mirrage::renderer
