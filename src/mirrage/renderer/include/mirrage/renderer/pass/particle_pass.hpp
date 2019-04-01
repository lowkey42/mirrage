#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/utils/random.hpp>


namespace mirrage::renderer {

	class Particle_pass_factory;

	/**
	 * @brief Updates particle systems and submits them for drawing
	 * Should be after animation but before other render_passes
	 */
	class Particle_pass : public Render_pass {
	  public:
		using Factory = Particle_pass_factory;

		Particle_pass(Deferred_renderer&, ecs::Entity_manager&);


		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Particle"; }

	  private:
		struct Update_uniform_buffer {
			graphic::Backed_buffer buffer;
			graphic::DescriptorSet desc_set;
			std::int32_t           capacity = -1;

			void reserve(Deferred_renderer& renderer, std::int32_t new_capacity);
		};
		struct Per_frame_data {
			vk::UniqueCommandBuffer            commands;
			graphic::Backed_buffer             particles;
			graphic::Backed_buffer             shared_uniforms;
			graphic::DescriptorSet             descriptor_set;
			std::int32_t                       capacity          = -1;
			std::int32_t                       effector_capacity = -1;
			std::vector<Update_uniform_buffer> particle_type_data;
			std::int32_t                       next_free_particle_type_data = 0;

			void reserve(Deferred_renderer& renderer,
			             std::int32_t       particle_count,
			             std::int32_t       particle_type_count,
			             std::int32_t       global_effector_count);

			auto next_particle_type_data() -> Update_uniform_buffer&;
		};

		struct Emitter_range {
			std::int32_t offset;
			std::int32_t count;
		};

		using Emitter_gpu_data = std::vector<std::weak_ptr<Particle_emitter_gpu_data>>;

		Deferred_renderer&            _renderer;
		ecs::Entity_manager&          _ecs;
		util::default_rand            _rand;
		vk::DeviceSize                _storage_buffer_offset_alignment;
		vk::UniqueDescriptorSetLayout _descriptor_set_layout;
		vk::UniquePipelineLayout      _pipeline_layout;
		std::uint64_t                 _rev = 0; //< used to invalidate data of old particle emitters

		float _dt = 0.f;

		bool                   _update_submitted = false;
		graphic::Fence         _update_fence;
		graphic::Backed_buffer _feedback_buffer;
		graphic::Backed_buffer _feedback_buffer_host;
		std::size_t            _feedback_buffer_size = 0;
		Emitter_gpu_data       _emitter_gpu_data;

		std::vector<Per_frame_data> _per_frame_data;
		std::int32_t                _current_frame = 0;
		bool                        _first_frame   = true;

		void _submit_update(Frame_data&);
		auto _alloc_feedback_buffer(Frame_data&)
		        -> std::tuple<gsl::span<Emitter_range>, gsl::span<std::uint32_t>>;
		void _update_descriptor_set(Per_frame_data&, util::maybe<Per_frame_data&>);
		void _update_type_uniforms(Frame_data&, Per_frame_data&);
		void _dispatch_emits(Frame_data&, vk::CommandBuffer);
		void _dispatch_updates(Frame_data&, vk::CommandBuffer);
	};

	class Particle_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Particle_pass_factory>();
		}

		auto create_pass(Deferred_renderer&,
		                 std::shared_ptr<void>,
		                 util::maybe<ecs::Entity_manager&>,
		                 Engine&,
		                 bool&) -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
