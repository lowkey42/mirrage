#include "test_animation_screen.hpp"

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/renderer/animation_comp.hpp>


namespace mirrage {
	using namespace ecs::components;
	using namespace util::unit_literals;
	using namespace graphic;

	Test_animation_screen::Test_animation_screen(Engine& engine) : Test_screen(engine)
	{
		_animation_test_dqs = _meta_system.entities()
		                              .entity_builder("monk")
		                              .position({-8, 0, -0.5f - 1.f})
		                              .direction(glm::vec3{-1, 0, 0})
		                              .create();

		_animation_test_lbs = _meta_system.entities()
		                              .entity_builder("monk_lbs")
		                              .position({-8, 0, -0.5f + 1.f})
		                              .direction(glm::vec3{-1, 0, 0})
		                              .create();


		_animation_test2_dqs = _meta_system.entities()
		                               .entity_builder("rotation_test")
		                               .position({-4, 0, -0.5f - 1.f})
		                               .create();

		_animation_test2_lbs = _meta_system.entities()
		                               .entity_builder("rotation_test_lbs")
		                               .position({-4, 0, -0.5f + 1.f})
		                               .create();
	}

	void Test_animation_screen::_draw()
	{
		auto anim_mb     = _animation_test_dqs.get<renderer::Simple_animation_controller_comp>();
		auto anim_lbs_mb = _animation_test_lbs.get<renderer::Simple_animation_controller_comp>();
		if(anim_mb.is_nothing())
			return;

		auto& anim = anim_mb.get_or_throw();

		auto ctx = _gui.ctx();
		if(nk_begin_titled(ctx,
		                   "Animation",
		                   "Animation",
		                   _gui.centered_right(300, 500),
		                   NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE)) {

			nk_layout_row_dynamic(ctx, 20, 1);

			nk_label(ctx, "Animation", NK_TEXT_LEFT);
			auto animations_strs = std::array<const char*, 9>{
			        {"[None]", "Attack", "Dance", "Die", "Flee", "Idle", "Sad", "Sleep", "Walk"}};
			auto animations_ids = std::array<util::Str_id, 9>{{""_strid,
			                                                   "attack"_strid,
			                                                   "dance"_strid,
			                                                   "die"_strid,
			                                                   "flee"_strid,
			                                                   "idle"_strid,
			                                                   "sad"_strid,
			                                                   "sleep"_strid,
			                                                   "walk"_strid}};
			(void) animations_ids;

			auto curr_animation_id = anim.animation_id().get_or(""_strid);
			auto curr_idx =
			        std::distance(animations_ids.begin(),
			                      std::find(animations_ids.begin(), animations_ids.end(), curr_animation_id));

			auto new_idx = nk_combo(ctx,
			                        animations_strs.data(),
			                        gsl::narrow<int>(animations_strs.size()),
			                        int(curr_idx),
			                        14,
			                        nk_vec2(100.f, 200));

			if(new_idx != curr_idx) {
				anim.play(animations_ids.at(std::size_t(new_idx)));
				anim_lbs_mb.process([&](auto& anim) { anim.play(animations_ids.at(std::size_t(new_idx))); });
			}

			anim.animation().process([&](auto& curr_animation) {
				auto& dqs_anim   = _animation_test_dqs.get<renderer::Animation_comp>().get_or_throw();
				auto& dqs_states = dqs_anim.states();
				auto& lbs_anim   = _animation_test_lbs.get<renderer::Animation_comp>().get_or_throw();
				auto& lbs_states = lbs_anim.states();

				auto time           = 0.f;
				auto curr_dqs_state = std::find_if(dqs_states.begin(), dqs_states.end(), [&](auto& s) {
					return &*s.animation == &*curr_animation;
				});
				auto curr_lbs_state = std::find_if(lbs_states.begin(), lbs_states.end(), [&](auto& s) {
					return &*s.animation == &*curr_animation;
				});

				if(curr_dqs_state != dqs_states.end()) {
					time = curr_dqs_state->time;
				} else if(curr_lbs_state != lbs_states.end()) {
					time = curr_dqs_state->time;
				}

				auto duration = curr_animation->duration();

				nk_label(ctx, "Time", NK_TEXT_LEFT);
				auto new_time = nk_slide_float(ctx, 0.f, time, duration, 0.01f);
				if(std::abs(new_time - time) > 0.00001f) {
					dqs_anim.mark_dirty();
					lbs_anim.mark_dirty();

					if(curr_dqs_state != dqs_states.end())
						curr_dqs_state->time = new_time;

					if(curr_lbs_state != lbs_states.end())
						curr_lbs_state->time = new_time;
				}

				nk_label(ctx,
				         (util::to_string(new_time) + " / " + util::to_string(duration)).c_str(),
				         NK_TEXT_LEFT);

				auto speed = anim.speed();
				nk_property_float(ctx, "Speed", 0.f, &speed, 5.f, 0.1f, 0.05f);
				anim.speed(speed);


				if(anim.paused())
					anim.pause(!nk_button_label(ctx, "Continue"));
				else
					anim.pause(nk_button_label(ctx, "Pause"));

				if(anim.reversed())
					anim.reverse(!nk_button_label(ctx, "Reverse (->)"));
				else
					anim.reverse(nk_button_label(ctx, "Reverse (<-)"));

				if(anim.looped())
					anim.loop(!nk_button_label(ctx, "Once"));
				else
					anim.loop(nk_button_label(ctx, "Repeat"));


				anim_lbs_mb.process([&](auto& anim_lbs) {
					anim_lbs.speed(anim.speed());
					anim_lbs.pause(anim.paused());
					anim_lbs.reverse(anim.reversed());
					anim_lbs.loop(anim.looped());
				});
			});
		}

		nk_label(ctx, "Rotation Test", NK_TEXT_LEFT);
		_animation_test2_dqs.get<renderer::Simple_animation_controller_comp>().process([&](auto& anim) {
			if(anim.paused())
				anim.pause(!nk_button_label(ctx, "Continue"));
			else
				anim.pause(nk_button_label(ctx, "Pause"));

			_animation_test2_lbs.get<renderer::Simple_animation_controller_comp>().process(
			        [&](auto& anim_lbs) { anim_lbs.pause(anim.paused()); });
		});

		nk_end(ctx);

		Test_screen::_draw();
	}

} // namespace mirrage
