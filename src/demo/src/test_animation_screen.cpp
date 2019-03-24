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

		ImGui::PositionNextWindow(
		        {300, 500}, ImGui::WindowPosition_X::right, ImGui::WindowPosition_Y::center);
		if(ImGui::Begin("Animation")) {
			ImGui::TextUnformatted("Animation");

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
			auto curr_idx          = int(std::distance(
                    animations_ids.begin(),
                    std::find(animations_ids.begin(), animations_ids.end(), curr_animation_id)));

			if(ImGui::Combo("Animation", &curr_idx, animations_strs.data(), int(animations_strs.size()))) {
				anim.play(animations_ids.at(std::size_t(curr_idx)));
				anim_lbs_mb.process([&](auto& anim) { anim.play(animations_ids.at(std::size_t(curr_idx))); });
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

				auto new_time = ImGui::ValueSliderFloat("Time", time, 0.f, duration);
				if(std::abs(new_time - time) > 0.00001f) {
					dqs_anim.mark_dirty();
					lbs_anim.mark_dirty();

					if(curr_dqs_state != dqs_states.end())
						curr_dqs_state->time = new_time;

					if(curr_lbs_state != lbs_states.end())
						curr_lbs_state->time = new_time;
				}

				ImGui::TextUnformatted(
				        (util::to_string(new_time) + " / " + util::to_string(duration)).c_str());

				anim.speed(ImGui::ValueSliderFloat("Speed", anim.speed(), 0.f, 5.f));


				if(anim.paused())
					anim.pause(!ImGui::Button("Continue"));
				else
					anim.pause(ImGui::Button("Pause"));

				ImGui::SameLine();

				if(anim.reversed())
					anim.reverse(!ImGui::Button("Reverse (->)"));
				else
					anim.reverse(ImGui::Button("Reverse (<-)"));

				ImGui::SameLine();

				if(anim.looped())
					anim.loop(!ImGui::Button("Once"));
				else
					anim.loop(ImGui::Button("Repeat"));


				anim_lbs_mb.process([&](auto& anim_lbs) {
					anim_lbs.speed(anim.speed());
					anim_lbs.pause(anim.paused());
					anim_lbs.reverse(anim.reversed());
					anim_lbs.loop(anim.looped());
				});
			});
		}

		ImGui::TextUnformatted("Rotation Test");
		_animation_test2_dqs.get<renderer::Simple_animation_controller_comp>().process([&](auto& anim) {
			if(anim.paused())
				anim.pause(!ImGui::Button("Continue"));
			else
				anim.pause(ImGui::Button("Pause"));

			_animation_test2_lbs.get<renderer::Simple_animation_controller_comp>().process(
			        [&](auto& anim_lbs) { anim_lbs.pause(anim.paused()); });
		});

		ImGui::End();

		Test_screen::_draw();
	}

} // namespace mirrage
