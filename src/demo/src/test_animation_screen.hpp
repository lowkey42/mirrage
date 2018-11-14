/** The main menu ************************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "test_screen.hpp"

#include <mirrage/engine.hpp>
#include <mirrage/gui/gui.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/utils/maybe.hpp>


namespace mirrage {

	class Test_animation_screen : public Test_screen {
	  public:
		Test_animation_screen(Engine& engine);

		auto name() const -> std::string override { return "Test_animation"; }

	  protected:
		void _draw() override;

		auto _prev_screen_policy() const noexcept -> Prev_screen_policy override
		{
			return Prev_screen_policy::discard;
		}

	  private:
		ecs::Entity_facet _animation_test_dqs;
		ecs::Entity_facet _animation_test_lbs;
		ecs::Entity_facet _animation_test2_dqs;
		ecs::Entity_facet _animation_test2_lbs;
	};
} // namespace mirrage
