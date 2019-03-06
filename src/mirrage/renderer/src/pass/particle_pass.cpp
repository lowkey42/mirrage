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
			std::int32_t padding[3];
			// + effector array
		};
		struct Uniform_Effector {
			glm::vec4 force_dir; // w=padding
			glm::vec4 position;  // w=padding

			float force;
			float distance_decay;
			float mass_scale; // used as a=mix(F/m, F, mass_scale)
			float fixed_dir;
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
			out.force          = effector.force;
			out.distance_decay = effector.distance_decay;
			out.mass_scale     = effector.scale_with_mass ? 1.f : 0.f;
			out.fixed_dir      = effector.fixed_dir ? 1.f : 0.f;
		}

		struct Update_uniforms {
			Random_value<Particle_color> color; // hsva
			Random_value<Particle_color> color_change;

			Random_value<glm::vec4> size;
			Random_value<glm::vec4> size_change;

			Random_value<float> sprite_rotation;
			float               padding1[2];
			Random_value<float> sprite_rotation_change;
			float               padding2[2];

			float base_mass;
			float density;
			float drag;
			float timestep;

			std::uint32_t particle_offset;
			std::uint32_t particle_count;
			std::int32_t  padding3;
			std::int32_t  effector_count;
			// + effectors
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
			auto create_info = vk::BufferCreateInfo{vk::BufferCreateFlags{},
			                                        size_bytes,
			                                        vk::BufferUsageFlagBits::eTransferDst
			                                                | vk::BufferUsageFlagBits::eStorageBuffer
			                                                | vk::BufferUsageFlagBits::eVertexBuffer,
			                                        vk::SharingMode::eConcurrent,
			                                        gsl::narrow<std::uint32_t>(allowed_queues.size()),
			                                        allowed_queues.data()};
			particles =
			        renderer.device().create_buffer(create_info, false, graphic::Memory_lifetime::temporary);
		}

		if(effector_capacity < global_effector_count) {
			effector_capacity = global_effector_count + 4;
			auto size_bytes =
			        vk::DeviceSize(effector_capacity) * effector_size_bytes + shared_uniforms_size_bytes;

			auto create_info = vk::BufferCreateInfo{
			        vk::BufferCreateFlags{},
			        size_bytes,
			        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer};
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

			auto size_bytes =
			        vk::DeviceSize(capacity) * effector_size_bytes + vk::DeviceSize(sizeof(Update_uniforms));

			auto create_info = vk::BufferCreateInfo{
			        vk::BufferCreateFlags{},
			        size_bytes,
			        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer};
			buffer = renderer.device().create_buffer(create_info, true, graphic::Memory_lifetime::normal);

			if(!desc_set)
				desc_set = renderer.create_descriptor_set(renderer.compute_uniform_buffer_layout(), 1);

			// update desc_set
			auto bufferInfo = vk::DescriptorBufferInfo{*buffer, 0, VK_WHOLE_SIZE};
			auto desc_write = vk::WriteDescriptorSet{
			        *desc_set, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo};

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
	            renderer.device().max_frames_in_flight(), [&](auto, auto& vec) {
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

			auto count_sum = 0;

			auto frame_idx =
			        _current_frame <= 0 ? _per_frame_data.size() - 1u : std::size_t(_current_frame - 1);
			for(auto i = std::uint32_t(0); i < _emitter_gpu_data.size(); i++) {
				auto& emitter_weak = _emitter_gpu_data[i];
				if(auto emitter = emitter_weak.lock(); emitter) {
					auto offset = feedback->offset;
					auto count  = feedback->count;
					count_sum += count;
					feedback++;
					emitter->set(&_rev, *_per_frame_data.at(frame_idx).particles, offset, count, i);
				}
			}

			LOG(plog::debug) << "Particle update done: " << count_sum;

			_emitter_gpu_data.clear();
		}

		if(!_update_submitted) {
			_submit_update(frame);
		}

		// TODO: move draw to other passes
		// TODO: find all particle_systems in draw-range
		// TODO:	draw them if they have a particle-buffer assigned
	}

	void Particle_pass::_submit_update(Frame_data& frame)
	{
		if(frame.particle_queue.empty())
			return;

		MIRRAGE_INVARIANT(_emitter_gpu_data.empty(),
		                  "_emitter_gpu_data is not empty at the start of the update process.");


		_sort_particles(frame);

		auto& data = _per_frame_data.at(std::size_t(_current_frame));

		data.commands = _renderer.create_compute_command_buffer();
		auto commands = *data.commands;
		commands.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

		auto [feedback, feedback_mapping] = _alloc_feedback_buffer(frame);

		// calc new particle count and assign the new range to each emitter
		auto last_type           = static_cast<const Particle_type_config*>(nullptr);
		auto particle_type_count = std::int32_t(0);
		auto new_particle_count  = std::int32_t(0);
		_emitter_gpu_data.reserve(frame.particle_queue.size());
		for(auto i = std::size_t(0); i < frame.particle_queue.size(); i++) {
			auto& p                          = frame.particle_queue[i];
			auto  spawn                      = p.emitter->spawn(_rand);
			auto  count                      = p.emitter->particle_count() + spawn;
			feedback[std::int32_t(i)].offset = new_particle_count;
			feedback[std::int32_t(i)].count  = spawn;
			if(p.emitter->particle_feedback_idx().is_some())
				feedback_mapping[p.emitter->particle_feedback_idx().get_or_throw()] = std::uint32_t(i);

			new_particle_count += count;

			_emitter_gpu_data.emplace_back(p.emitter->gpu_data());

			if(last_type != &*p.emitter->cfg().type) {
				last_type = &*p.emitter->cfg().type;
				particle_type_count++;
			}
		}


		// resize/create new particle_buffer
		auto effector_count = _ecs.list<Particle_effector_comp>().size();
		data.reserve(_renderer, new_particle_count, particle_type_count, effector_count);

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
		auto& shared_uniforms          = *reinterpret_cast<Shared_uniform_data*>(shared_uniforms_ptr);
		shared_uniforms.effector_count = std::int32_t(effector_count);
		auto uniform_effectors         = gsl::span<Uniform_Effector>(
                reinterpret_cast<Uniform_Effector*>(shared_uniforms_ptr + sizeof(Shared_uniform_data)),
                effector_count);

		for(auto [i, e, t] : util::with_index(_ecs.list<Particle_effector_comp, Transform_comp>())) {
			effector_to_uniform(e.effector, t.position, t.orientation, uniform_effectors[i]);
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

		_dispatch_updates(frame, commands, data);

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

	void Particle_pass::_sort_particles(Frame_data& frame)
	{
		// sort by particle type, effectors and draw mask for optimal update/draw batching
		std::sort(frame.particle_queue.begin(), frame.particle_queue.end(), [&](auto& lhs, auto& rhs) {
			auto lhs_type = &*lhs.emitter->cfg().type;
			auto rhs_type = &*lhs.emitter->cfg().type;

			if(lhs_type < rhs_type)
				return true;
			else if(lhs_type > rhs_type)
				return false;

			if(lhs.effectors.empty() && rhs.effectors.empty())
				return lhs.culling_mask < rhs.culling_mask;
			else
				return lhs.effectors.data() < rhs.effectors.data();
		});
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
		desc_writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
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

	namespace {
		struct Emitter_push_constants {
			glm::vec4 parent_velocity;
			glm::vec4 position;
			glm::quat rotation_quat;

			float ttl_mean;
			float ttl_stddev;
			float velocity_mean;
			float velocity_stddev;

			std::uint32_t offset;
			std::uint32_t to_spawn;
			std::uint32_t base_seed;
			std::uint32_t feedback_buffer_id;
			float         timestep;
		};
	} // namespace

	void Particle_pass::_dispatch_updates(Frame_data& frame, vk::CommandBuffer commands, Per_frame_data& data)
	{
		const auto dt = _dt;

		data.next_free_particle_type_data = 0;

		// submit emits and updates
		auto last_particle = &frame.particle_queue.front();
		auto last_type     = &*last_particle->emitter->cfg().type;
		auto range_begin   = std::int32_t(0);
		auto range_count   = std::int32_t(0);

		auto submit_update = [&](auto begin, auto count, auto& p) {
			MIRRAGE_INVARIANT(begin >= range_begin, begin << " < " << range_begin);
			range_begin = begin + count;
			range_count = 0;

			if(count > 0) {
				const auto& cfg            = *p.emitter->cfg().type;
				auto        effector_count = p.system->effectors().size();

				// update uniforms
				auto& type_data = data.next_particle_type_data();
				type_data.reserve(_renderer, std::int32_t(effector_count));

				auto uniforms_ptr =
				        reinterpret_cast<char*>(type_data.buffer.memory().mapped_addr().get_or_throw(
				                "particle type uniform buffer is not mapped"));
				auto& uniforms                  = *reinterpret_cast<Update_uniforms*>(uniforms_ptr);
				uniforms.color                  = cfg.color;
				uniforms.color_change           = cfg.color_change;
				uniforms.size                   = cfg.size;
				uniforms.size_change            = cfg.size_change;
				uniforms.sprite_rotation        = cfg.sprite_rotation;
				uniforms.sprite_rotation_change = cfg.sprite_rotation_change;
				uniforms.base_mass              = cfg.base_mass;
				uniforms.density                = cfg.density;
				uniforms.drag                   = cfg.drag;
				uniforms.timestep               = dt;
				uniforms.particle_offset        = std::uint32_t(begin);
				uniforms.particle_count         = std::uint32_t(count);
				uniforms.effector_count         = std::int32_t(effector_count);


				auto uniform_effectors = gsl::span<Uniform_Effector>(
				        reinterpret_cast<Uniform_Effector*>(uniforms_ptr + sizeof(Update_uniforms)),
				        std::int32_t(effector_count));

				for(auto [i, e] : util::with_index(p.system->effectors())) {
					effector_to_uniform(e, p.system->position(), p.system->rotation(), uniform_effectors[i]);
				}

				commands.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
				                            *_pipeline_layout,
				                            1u,
				                            1u,
				                            type_data.desc_set.get_ptr(),
				                            0,
				                            nullptr);

				cfg.update_script->bind(commands);

				// dispatch update READ from old particle_buffer APPEND to new particle_buffer and feedback_buffer
				auto groups = static_cast<std::uint32_t>(std::ceil(float(count) / update_workgroup_size));
				commands.dispatch(groups, 1, 1);
			}
		};

		auto idx    = std::int32_t(0);
		auto offset = std::int32_t(0);

		for(auto& p : frame.particle_queue) {
			auto& type = *p.emitter->cfg().type;

			const auto to_spawn = p.emitter->particles_to_spawn();
			const auto count    = p.emitter->particle_count() + to_spawn;

			if(to_spawn > 0) {
				// dispatch emit WRITE to new particle_buffer and feedback_buffer
				const auto& cfg = p.emitter->cfg();
				p.emitter->cfg().emit_script->bind(commands);

				auto pcs            = Emitter_push_constants{};
				pcs.parent_velocity = glm::vec4(
				        (p.system->position() - p.system->_last_position) / dt * cfg.parent_velocity, 0.f);
				pcs.position      = glm::vec4(p.system->emitter_position(*p.emitter), 0.f);
				pcs.rotation_quat = p.system->emitter_rotation(*p.emitter);

				pcs.ttl_mean        = cfg.ttl.mean;
				pcs.ttl_stddev      = cfg.ttl.stddev;
				pcs.velocity_mean   = cfg.velocity.mean;
				pcs.velocity_stddev = cfg.velocity.stddev;

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

			if(!p.effectors.empty()) {
				submit_update(range_begin, range_count, *last_particle);
				if(last_particle != &p)
					submit_update(offset, count, p);

			} else if(&type != last_type) {
				submit_update(range_begin, range_count, *last_particle);

			} else {
				// append to current batch
				range_count += count;
			}

			last_particle = &p;
			last_type     = &type;
			idx++;
			offset += std::uint32_t(p.emitter->particles_to_spawn());
		}

		submit_update(range_begin, range_count, *last_particle);
	}


	auto Particle_pass_factory::create_pass(Deferred_renderer&                renderer,
	                                        util::maybe<ecs::Entity_manager&> ecs,
	                                        Engine&,
	                                        bool&) -> std::unique_ptr<Render_pass>
	{
		if(ecs.is_nothing())
			return {};

		return std::make_unique<Particle_pass>(renderer, ecs.get_or_throw());
	}

	auto Particle_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Particle_pass_factory::configure_device(vk::PhysicalDevice,
	                                             util::maybe<std::uint32_t>,
	                                             graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
