#pragma once

#include <mirrage/renderer/animation.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/streamed_buffer.hpp>

#include <tsl/robin_map.h>
#include <gsl/gsl>


namespace mirrage::graphic {
	struct Pipeline_description;
	class Subpass_builder;
} // namespace mirrage::graphic

namespace mirrage::renderer {
	class Pose_comp;

	class Deferred_geometry_subpass {
	  public:
		Deferred_geometry_subpass(Deferred_renderer&, ecs::Entity_manager&);

		void configure_pipeline(Deferred_renderer&, graphic::Pipeline_description&);
		void configure_subpass(Deferred_renderer&, graphic::Subpass_builder&);

		void configure_animation_pipeline(Deferred_renderer&, graphic::Pipeline_description&);
		void configure_animation_subpass(Deferred_renderer&, graphic::Subpass_builder&);

		void update(util::Time dt);
		void pre_draw(Frame_data&);
		void draw(Frame_data&, graphic::Render_pass&);

	  private:
		struct Animation_upload_queue_entry {
			const Model*     model;
			const Pose_comp* pose;
			std::int32_t     uniform_offset;

			Animation_upload_queue_entry(const Model* model, Pose_comp& pose, std::int32_t uniform_offset)
			  : model(model), pose(&pose), uniform_offset(uniform_offset)
			{
			}
		};

		ecs::Entity_manager& _ecs;
		Deferred_renderer&   _renderer;

		vk::UniqueDescriptorSetLayout       _animation_desc_layout;
		graphic::Streamed_buffer            _animation_uniforms;
		std::vector<graphic::DescriptorSet> _animation_desc_sets;

		util::iter_range<std::vector<Geometry>::iterator> _geometry_range;
		util::iter_range<std::vector<Geometry>::iterator> _rigged_geometry_range;

		std::unordered_map<ecs::Entity_handle, std::uint32_t> _animation_uniform_offsets;
		std::vector<Animation_upload_queue_entry>             _animation_uniform_queue;
	};
} // namespace mirrage::renderer
