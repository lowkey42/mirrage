#pragma once

#include <mirrage/renderer/animation_comp.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/streamed_buffer.hpp>

#include <tsl/robin_map.h>
#include <gsl/gsl>


namespace mirrage::renderer::detail {
	struct Animation_key_cache_key {
		ecs::Entity_handle owner;
		util::Str_id       id;

		auto operator==(const Animation_key_cache_key& rhs) const noexcept
		{
			return std::tie(owner, id) == std::tie(rhs.owner, rhs.id);
		}
		auto operator!=(const Animation_key_cache_key& rhs) const noexcept { return !(*this == rhs); }
		auto operator<(const Animation_key_cache_key& rhs) const noexcept
		{
			return std::tie(owner, id) < std::tie(rhs.owner, rhs.id);
		}
	};
} // namespace mirrage::renderer::detail

namespace std {
	template <>
	struct hash<mirrage::renderer::detail::Animation_key_cache_key> {
		size_t operator()(const mirrage::renderer::detail::Animation_key_cache_key& key) const noexcept
		{
			return 71 * hash<mirrage::ecs::Entity_handle>()(key.owner)
			       + hash<mirrage::util::Str_id>()(key.id);
		}
	};
} // namespace std

namespace mirrage::renderer {

	class Animation_pass_factory;

	class Animation_pass : public Render_pass {
	  public:
		using Factory = Animation_pass_factory;

		Animation_pass(Deferred_renderer&, ecs::Entity_manager&);

		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Animation"; }

	  private:
		using Animation_key_cache =
		        tsl::robin_map<detail::Animation_key_cache_key, util::small_vector<Animation_key, 60>>;

		Deferred_renderer&   _renderer;
		ecs::Entity_manager& _ecs;

		// data for animation/pose update
		std::unordered_set<detail::Animation_key_cache_key> _unused_animation_keys;
		Animation_key_cache                                 _animation_key_cache;

		// data for pose upload
		struct Animation_upload_queue_entry {
			const Model*     model;
			const Pose_comp* pose;
			std::int32_t     uniform_offset;

			Animation_upload_queue_entry(const Model* model, Pose_comp& pose, std::int32_t uniform_offset)
			  : model(model), pose(&pose), uniform_offset(uniform_offset)
			{
			}
		};

		graphic::Streamed_buffer            _animation_uniforms;
		std::vector<graphic::DescriptorSet> _animation_desc_sets;

		tsl::robin_map<ecs::Entity_handle, std::uint32_t> _animation_uniform_offsets;
		std::vector<Animation_upload_queue_entry>         _animation_uniform_queue;

		void _update_animation(ecs::Entity_handle owner, Animation_comp& anim, Pose_comp&);
		void _compute_poses(Frame_data&);
		void _upload_poses(Frame_data&);
	};

	class Animation_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Animation_pass_factory>();
		}

		auto create_pass(Deferred_renderer&, util::maybe<ecs::Entity_manager&>, Engine&, bool&)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
